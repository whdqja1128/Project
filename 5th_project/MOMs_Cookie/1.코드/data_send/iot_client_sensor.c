//IOT_CLIENT_SENSOR 코드 변경 전
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <mysql/mysql.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];
struct sockaddr_in serv_addr;

int main(int argc, char* argv[])
{
   int sock;
   
   pthread_t snd_thread, rcv_thread, mysql_thread;
   void* thread_return;

   if (argc != 4) {
      printf("Usage : %s <IP> <port> <name>\n", argv[0]);
      exit(1);
   }

   sprintf(name, "%s", argv[3]);

   sock = socket(PF_INET, SOCK_STREAM, 0);
   if (sock == -1)
      error_handling("socket() error");

   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
   serv_addr.sin_port = htons(atoi(argv[2]));

   if (connect(sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1)
      error_handling("connect() error");

   sprintf(msg, "%s", name);
   write(sock, msg, strlen(msg));
   pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
   pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);

   pthread_join(snd_thread, &thread_return);
   pthread_join(rcv_thread, &thread_return);

   close(sock);
   return 0;
}

void* send_msg(void* arg)
{
   MYSQL* conn;
   MYSQL_ROW sqlrow;
   MYSQL_RES* res;
   char sql_cmd[200] = { 0 };
   char* host = "localhost";
   char* user = "iot";
   char* pass = "pwiot";
   char* dbname = "iotdb";

   int* sock = (int*)arg;
   int str_len;
   int ret;
   fd_set initset, newset;
   struct timeval tv;
   char name_msg[NAME_SIZE + BUF_SIZE + 2];

   conn = mysql_init(NULL);
   puts("MYSQL condition start\n\n");
   if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
   {
      fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
      exit(1);
   }
   else
      printf("Connection Successful!\n\n");

   FD_ZERO(&initset);
   FD_SET(STDIN_FILENO, &initset);

   fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
   while (1) {
      memset(msg, 0, sizeof(msg));
      name_msg[0] = '\0';
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      newset = initset;
      ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);

      sprintf(sql_cmd,"SELECT time, temp, humi, water FROM sensor ORDER BY id DESC LIMIT 1");
      if (mysql_query(conn, sql_cmd))
      {
         fprintf(stderr, "MySQL Query Error: %s\n", mysql_error(conn));
         continue;
      }
      
      res = mysql_store_result(conn);
      if ((sqlrow = mysql_fetch_row(res)))
      {         
      char* time_str = sqlrow[0];
      double temp = atof(sqlrow[1]);
      int humi = atoi(sqlrow[2]);
      int water = atoi(sqlrow[3]);

      //printf("최신값 : 온도=%.1lf℃, 습도 = %d 물 = %d\n", temp, humi, water);
      // printf("[DEBUG] sqlrow[0] (raw): '%s'\n", sqlrow[0]);
         // if(water <= 300 && humi < 30)
         // {
         //    printf("가습기 ON : 습도 = %d 물의 양 = %d\r\n",humi,water);

         //    sprintf(name_msg, "[USR_BT]humidifierON\n]");
         
            if (write(*sock, name_msg,strlen(name_msg)) <= 0)
            {
               *sock = -1;
               mysql_free_result(res);
               return NULL;
            }

         // }
        

      }
      mysql_free_result(res);
      
      if(ret == 0) {
         if (*sock == -1){
            mysql_close(conn);
            return NULL;
         }
      }

      if (FD_ISSET(STDIN_FILENO, &newset))
      {
         fgets(msg, BUF_SIZE, stdin);
         if (!strncmp(msg, "quit\n", 5)) {
            *sock = -1;
            return NULL;
         }
         else if (msg[0] != '[')
         {
            strcat(name_msg, "[ALLMSG]");
            strcat(name_msg, msg);
         }
         else
            strcpy(name_msg, msg);
         if (write(*sock, name_msg, strlen(name_msg)) <= 0)
         {
            *sock = -1;
            return NULL;
         }
      }
      if (ret == 0)
      {
         if (*sock == -1)
            return NULL;
      }
   }
}

void* recv_msg(void* arg)
{
   MYSQL* conn;
   MYSQL_ROW sqlrow;
   int res;
   char sql_cmd[200] = { 0 };
   char* host = "10.10.141.73";
   char* user = "iot";
   char* pass = "pwiot";
   char* dbname = "iotdb";

   int* sock = (int*)arg;
   int i;
   char* pToken;
   char* pArray[ARR_CNT] = { 0 };

   char name_msg[NAME_SIZE + BUF_SIZE + 1];
   int str_len;

   double temp;
   double humi;
   int water;

   conn = mysql_init(NULL);
   puts("MYSQL startup");

   if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
   {
      fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
      exit(1);
   }
   else
      printf("Connection Successful!\n\n");

   while (1) {

      memset(name_msg, 0x0, sizeof(name_msg));
      str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);

      if (str_len <= 0)
      {
         printf("⚠️ 연결 끊김 또는 read 에러, 재연결 시도 중...\n");
         close(*sock);
         *sock = -1;

         // 재연결 시도
         while (*sock == -1) {
            *sock = socket(PF_INET, SOCK_STREAM, 0);
            if (*sock == -1) {
               perror("socket() 실패");
               sleep(2);
               continue;
            }

            if (connect(*sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
               perror("connect() 실패");
               close(*sock);
               *sock = -1;
               sleep(2);
               continue;
            }

            printf("✅ 소켓 재연결 성공\n");
            write(*sock, name, strlen(name));  // 로그인 다시 전송
         }

         continue;
      }

      name_msg[str_len] = 0;
      fputs(name_msg, stdout);

      // "GET@TIME" 요청 처리
      if (strncmp(name_msg, "[USR_STM32]GET@TIME", 18) == 0) {
         printf("[INFO] GET@TIME 요청 받음. 현재 시간 조회 중...\n");

         // MySQL에서 현재 시간 가져오기
         if (mysql_query(conn, "SELECT NOW()")) {
            fprintf(stderr, "MySQL Query Error: %s\n", mysql_error(conn));
            //continue;
         }

         MYSQL_RES* res = mysql_store_result(conn);
         MYSQL_ROW row = mysql_fetch_row(res);

         if (row) {
            char time_msg[BUF_SIZE];
            char datehour[5];
            char datemin[5];
            char datesec[5];
            strncpy(datehour,row[0]+11,2);
            strncpy(datemin,row[0]+14,2);
            strncpy(datesec,row[0]+17,2);
            
            datehour[2] = '\0';
            datemin[2] = '\0';
            datesec[2] = '\0';

            sprintf(time_msg, "[USR_STM32]TIME@%s-%s-%s\n", datehour,datemin,datesec);  // "[USR_STM32]YYYY-MM-DD HH:MM:SS"
            printf("[INFO] STM32로 현재 시간 전송: %s\n", time_msg);

            if (write(*sock, time_msg, strlen(time_msg)) <= 0) {
               perror("[ERROR] STM32로 시간 전송 실패 (write 실패)");
               *sock = -1;
            }
           
         }

         mysql_free_result(res);
      }

      // 기존 센서 데이터 처리 로직
      pToken = strtok(name_msg, "[:@]");
      i = 0;
      while (pToken != NULL)
      {
         pArray[i] = pToken;

         if (++i >= ARR_CNT)
            break;
         pToken = strtok(NULL, "[:@]");
      }

      printf("[TOKEN] i = %d, pArray[1] = %s\n", i, pArray[1]);

      if (pArray[1] && !strcmp(pArray[1], "SENSOR") && (i == 5)) {
         temp = (int)(atof(pArray[2]) * 0.95 + 0.5);
         humi = atof(pArray[3]);
         water = atof(pArray[4]);

         sprintf(sql_cmd, "insert into sensor(name, date, time, temp, humi, water) values(\"%s\",now(),now(),%lf,%lf,%d)",
                 pArray[0], temp, humi, water);

         res = mysql_query(conn, sql_cmd);
         if (!res)
            printf("inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
         else
            fprintf(stderr, "ERROR: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
      }

   }

   mysql_close(conn);
}

void error_handling(char* msg)
{
   fputs(msg, stderr);
   fputc('\n', stderr);
   exit(1);
}