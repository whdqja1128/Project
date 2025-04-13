#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define BUF_SIZE 4096
#define MAX_CLNT 32
#define ID_SIZE 10
#define ARR_CNT 5

#define DEBUG
typedef struct {
      char fd;
      char *from;
      char *to;
      char *msg;
      int len;
}MSG_INFO;

typedef struct {
    int index;
    int fd;
    char ip[20];
    char id[ID_SIZE];
    // pw 필드 제거됨
} CLIENT_INFO;

void * clnt_connection(void * arg);
void send_msg(MSG_INFO * msg_info, CLIENT_INFO * first_client_info);
void error_handling(char * msg);
void log_file(char * msgstr);
void getlocaltime(char * buf);
void cleanup_expired_schedules();

int clnt_cnt=0;
pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    int sock_option = 1;
    pthread_t t_id[MAX_CLNT] = {0};
    int str_len = 0;
    int i;
    char idpasswd[(ID_SIZE*2)+3];
    char *pToken;
    char *pArray[ARR_CNT]={0};
    char msg[BUF_SIZE];

    CLIENT_INFO client_info[MAX_CLNT] = {{0,-1,"","USR_API"}, \
            {0,-1,"","USR_SQL"},  {0,-1,"","USR_BT"}, \
            {0,-1,"","USR_STM32"},  {0,-1,"","USR_LIN"}, \
            {0,-1,"","USR_AND"},  {0,-1,"","USR_ARD"}, \
            {0,-1,"","HM_CON"}, {0,-1,"","USR_AI"}, \
            {0,-1,"","Weather"}, {0,-1,"","1"}};
            
    if(argc != 2) {
        printf("Usage : %s <port>\n",argv[0]);
        exit(1);
    }

    fputs("IoT Server Start!!\n",stdout);

    // 날짜가 지난 일정 정리
    cleanup_expired_schedules();

    if(pthread_mutex_init(&mutx, NULL))
        error_handling("mutex init error");

    if(pthread_mutex_init(&mutx, NULL))
        error_handling("mutex init error");

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // 소켓 옵션 설정 (재사용 가능)
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &sock_option, sizeof(sock_option));

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_adr.sin_port=htons(atoi(argv[1]));

    // 서버 주소에 바인딩
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 연결 대기 상태로 변경
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 클라이언트 연결 수신 루프
    while(1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if(clnt_sock == -1)
            continue;

        // 클라이언트로부터 ID/비밀번호 수신
        str_len = read(clnt_sock, idpasswd, sizeof(idpasswd));
        idpasswd[str_len] = '\0';

        if(str_len > 0)
        {
            // 입력된 ID만 사용 (비밀번호 부분은 모두 제거)
            char *device_id = idpasswd;
            
            // 콜론(:)이 포함된 경우 해당 부분까지만 ID로 인식
            char *colon_pos = strchr(idpasswd, ':');
            if(colon_pos != NULL) {
                *colon_pos = '\0';
            }
            
            for(i=0;i<MAX_CLNT;i++)
            {
                // main 함수의 클라이언트 인증 및 연결 처리 부분 수정
                if(client_info[i].id[0] != '\0' && !strcmp(client_info[i].id, device_id))
                {
                    if(client_info[i].fd != -1)
                    {
                        // 기존: Already logged 메시지 보내고 거부
                        // 수정: 기존 연결 강제 종료하고 새 연결 허용
                        sprintf(msg,"[%s] Existing connection terminated, allowing new connection\n", device_id);
                        log_file(msg);
        
                        // 기존 연결 소켓 닫기
                        close(client_info[i].fd);
        
                        // 클라이언트 카운트 감소 (이후 다시 증가됨)
                        pthread_mutex_lock(&mutx);
                        if(clnt_cnt > 0) clnt_cnt--;
                        pthread_mutex_unlock(&mutx);
                    }
    
                    // 이제 새 연결 수락 진행 (기존 코드와 동일)
                    strcpy(client_info[i].ip, inet_ntoa(clnt_adr.sin_addr));
                    pthread_mutex_lock(&mutx);
                    client_info[i].index = i; 
                    client_info[i].fd = clnt_sock; 
                    clnt_cnt++;
                    pthread_mutex_unlock(&mutx);
                    sprintf(msg,"[%s] New connected! (ip:%s,fd:%d,sockcnt:%d)\n",
                            device_id, inet_ntoa(clnt_adr.sin_addr), clnt_sock, clnt_cnt);
                    log_file(msg);
                    write(clnt_sock, msg, strlen(msg));
                    
                    pthread_create(t_id+i, NULL, clnt_connection, (void *)(client_info + i));
                    pthread_detach(t_id[i]);
                    break;
                }
            }
            
            // ID 찾지 못한 경우 인증 오류 메시지
            if(i == MAX_CLNT)
            {
                sprintf(msg,"[%s] Authentication Error!\n", device_id);
                write(clnt_sock, msg, strlen(msg));
                log_file(msg);
                shutdown(clnt_sock, SHUT_WR);
            }
        }
    }
    
    close(serv_sock);
    return 0;
}

void * clnt_connection(void *arg)
{
    CLIENT_INFO * client_info = (CLIENT_INFO *)arg;
    int str_len = 0;
    int index = client_info->index;
    char msg[BUF_SIZE];
    char to_msg[MAX_CLNT*ID_SIZE+1];
    int i=0;
    char *pToken;
    char *pArray[ARR_CNT]={0};
    char strBuff[130]={0};

    MSG_INFO msg_info;
    CLIENT_INFO  * first_client_info;

    first_client_info = (CLIENT_INFO *)((void *)client_info - (void *)( sizeof(CLIENT_INFO) * index ));
    while(1)
    {
        memset(msg,0x0,sizeof(msg));
        str_len = read(client_info->fd, msg, sizeof(msg)-1); 
        if(str_len <= 0)
            break;

        msg[str_len] = '\0';
        pToken = strtok(msg,"[:]");
        i = 0; 
        while(pToken != NULL)
        {
            pArray[i] =  pToken;
            if(i++ >= ARR_CNT)
                break;  
            pToken = strtok(NULL,"[:]");
        }

        // 메시지 파싱 후 필수 항목 누락 체크
      if (pArray[0] == NULL || pArray[1] == NULL) {

            continue;  // 다음 메시지로 넘어감
      }

        msg_info.fd = client_info->fd;
        msg_info.from = client_info->id;
        msg_info.to = pArray[0];
        // 날씨 명령어 처리
        // 날씨 명령어 처리 부분을 다음과 같이 수정
if (strstr(pArray[1], "날씨정보") != 0) {
    // 파일이 존재하는지 확인'
    FILE *weather_file = fopen("weather_data.txt", "r");
    if (weather_file != NULL) {
        // 각 줄을 개별적으로 읽고 전송하는 방식 적용
        char line1[256] = {0}; // 첫 번째 줄 - 날짜/지역 정보
        char line2[256] = {0}; // 두 번째 줄 - 온도/하늘/강수확률 등
        char line3[256] = {0}; // 세 번째 줄 - 미세먼지 정보
        
        // 각 줄 읽기
        fgets(line1, sizeof(line1), weather_file);
        fgets(line2, sizeof(line2), weather_file);
        fgets(line3, sizeof(line3), weather_file);
        fclose(weather_file);
        
        // 개행 문자 제거
        line1[strcspn(line1, "\n")] = 0;
        line2[strcspn(line2, "\n")] = 0;
        line3[strcspn(line3, "\n")] = 0;
        
        // 클라이언트 검색
        for(i=0; i<MAX_CLNT; i++) {
            if((first_client_info+i)->fd != -1) {
                if(!strcmp(msg_info.to, (first_client_info+i)->id)) {
                    int fd = (first_client_info+i)->fd;
                    
                    // 첫 번째 줄 전송
                    char msg1[512];
                    sprintf(msg1, "%s\n", line1);
                    write(fd, msg1, strlen(msg1));
                    usleep(100000); // 0.1초 대기
                    
                    // 두 번째 줄 전송 
                    char msg2[512];
                    sprintf(msg2, "%s\n", line2);
                    write(fd, msg2, strlen(msg2));
                    usleep(100000); // 0.1초 대기
                    
                    // 세 번째 줄 전송
                    char msg3[512];
                    sprintf(msg3, "%s", line3);
                    write(fd, msg3, strlen(msg3));
                    
                    sprintf(strBuff, "날씨 정보 3줄 각각 전송 완료");
                    log_file(strBuff);
                    break;
                }
            }
        }
        continue;
    }
    
            // 오류 시 기존 처리 사용
            sprintf(to_msg, "[%s]날씨 정보를 가져올 수 없습니다.", msg_info.from);
            msg_info.msg = to_msg;
            msg_info.len = strlen(to_msg);
            send_msg(&msg_info, first_client_info);
            continue;
        }

// 일정 파일 전송 시작
if (strcmp(pArray[1], "SCHEDULE_FILE_START") == 0) {
    // 이미 전송 중인 임시 파일이 있는지 확인하고 제거
    if (access("schedule_temp.txt", F_OK) != -1) {
        remove("schedule_temp.txt");
        sprintf(strBuff, "기존 임시 파일 제거");
        log_file(strBuff);
    }
    
    // 새 임시 파일 생성
    FILE *temp_file = fopen("schedule_temp.txt", "w");
    if (temp_file != NULL) {
        fclose(temp_file);
        sprintf(strBuff, "일정 파일 전송 시작 - 새 임시 파일 생성");
        log_file(strBuff);
        
        // 시작 확인 메시지 전송
        sprintf(to_msg, "[%s]일정 파일 전송 시작 - 준비 완료", msg_info.from);
        msg_info.msg = to_msg;
        msg_info.len = strlen(to_msg);
        send_msg(&msg_info, first_client_info);
    } else {
        sprintf(strBuff, "임시 일정 파일 생성 실패");
        log_file(strBuff);
        
        sprintf(to_msg, "[%s]일정 파일 전송 시작 실패 - 파일 생성 오류", msg_info.from);
        msg_info.msg = to_msg;
        msg_info.len = strlen(to_msg);
        send_msg(&msg_info, first_client_info);
    }
    continue;
}
// 일정 파일 내용 받기
else if (strstr(pArray[1], "SCHEDULE_FILE_CONTENT") == pArray[1]) {
    // 메시지 형식: SCHEDULE_FILE_CONTENT:일정데이터
    char *schedule_data = pArray[1] + strlen("SCHEDULE_FILE_CONTENT:");
    
    if (strlen(schedule_data) > 0) {
        // 임시 일정 파일에 내용 추가
        FILE *temp_file = fopen("schedule_temp.txt", "a");
        if (temp_file != NULL) {
            fprintf(temp_file, "%s\n", schedule_data);
            fclose(temp_file);
            
            sprintf(strBuff, "일정 데이터 수신: %s", schedule_data);
            log_file(strBuff);
        } else {
            sprintf(strBuff, "임시 일정 파일 열기 실패");
            log_file(strBuff);
            
            sprintf(to_msg, "[%s]일정 데이터 저장 실패: 파일 열기 오류", msg_info.from);
            msg_info.msg = to_msg;
            msg_info.len = strlen(to_msg);
            send_msg(&msg_info, first_client_info);
        }
    }
    continue;
}
// 일정 파일 전송 완료
else if (strcmp(pArray[1], "SCHEDULE_FILE_END") == 0) {
    // 기존 파일 백업 (있는 경우)
    if (access("schedule.txt", F_OK) != -1) {
        rename("schedule.txt", "schedule.bak");
        sprintf(strBuff, "기존 일정 파일 백업");
        log_file(strBuff);
    }
    
    // 중복 일정 확인 및 제거
    char unique_lines[1000][256]; // 최대 1000개 라인 저장 가능한 배열
    int unique_count = 0;
    
    FILE *temp_file = fopen("schedule_temp.txt", "r");
    if (temp_file != NULL) {
        char line[256];
        
        // 임시 파일에서 모든 라인 읽기
        while (fgets(line, sizeof(line), temp_file) && unique_count < 1000) {
            // 개행 문자 제거
            line[strcspn(line, "\n")] = 0;
            
            // 이미 있는지 확인
            int is_duplicate = 0;
            for (int i = 0; i < unique_count; i++) {
                if (strcmp(unique_lines[i], line) == 0) {
                    is_duplicate = 1;
                    break;
                }
            }
            
            // 중복이 아니면 추가
            if (!is_duplicate && strlen(line) > 0) {
                strcpy(unique_lines[unique_count], line);
                unique_count++;
            }
        }
        
        fclose(temp_file);
        
        // 고유한 라인만 포함하는 새 임시 파일 작성
        temp_file = fopen("schedule_temp.txt", "w");
        if (temp_file != NULL) {
            for (int i = 0; i < unique_count; i++) {
                fprintf(temp_file, "%s\n", unique_lines[i]);
            }
            fclose(temp_file);
            
            // 임시 파일을 실제 파일로 이동
            if (rename("schedule_temp.txt", "schedule.txt") == 0) {
                sprintf(strBuff, "일정 파일 업데이트 완료 (%d개 일정)", unique_count);
                log_file(strBuff);
                
                sprintf(to_msg, "[%s]일정 파일이 성공적으로 업데이트되었습니다. (%d개 일정)", 
                        msg_info.from, unique_count);
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            } else {
                sprintf(strBuff, "일정 파일 업데이트 실패: %s", strerror(errno));
                log_file(strBuff);
                
                sprintf(to_msg, "[%s]일정 파일 업데이트 실패", msg_info.from);
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            }
        } else {
            sprintf(strBuff, "임시 파일 다시 열기 실패");
            log_file(strBuff);
            
            sprintf(to_msg, "[%s]일정 파일 처리 중 오류 발생", msg_info.from);
            msg_info.msg = to_msg;
            msg_info.len = strlen(to_msg);
            send_msg(&msg_info, first_client_info);
        }
    } else {
        sprintf(strBuff, "임시 파일 열기 실패");
        log_file(strBuff);
        
        sprintf(to_msg, "[%s]일정 파일 처리 중 오류 발생", msg_info.from);
        msg_info.msg = to_msg;
        msg_info.len = strlen(to_msg);
        send_msg(&msg_info, first_client_info);
    }
    continue;
}
// 일정 명령어 처리
else if (strstr(pArray[1], "일정정보") != 0) {
    FILE *schedule_file = fopen("schedule.txt", "r");
    if (schedule_file != NULL) {
        char line[256] = {0};
        char all_contents[4096] = {0}; // 전체 내용을 저장할 버퍼
        
        // 파일의 끝까지 모든 줄을 읽어서 하나의 문자열로 합침
        while (fgets(line, sizeof(line), schedule_file) != NULL) {
            strcat(all_contents, line);
        }
        fclose(schedule_file);
        
        // 클라이언트 검색
        for(i=0; i<MAX_CLNT; i++) {
            if((first_client_info+i)->fd != -1) {
                if(!strcmp(msg_info.to, (first_client_info+i)->id)) {
                    int fd = (first_client_info+i)->fd;
                    
                    // 모든 내용을 한 번에 전송
                    write(fd, all_contents, strlen(all_contents));
                    
                    sprintf(strBuff, "일정 정보 전체 내용 전송 완료");
                    log_file(strBuff);
                    break;
                }
            }
        }
        continue;
    }
    
    // 오류 시 기존 처리 사용
    sprintf(to_msg, "[%s]일정 정보를 가져올 수 없습니다.", msg_info.from);
    msg_info.msg = to_msg;
    msg_info.len = strlen(to_msg);
    send_msg(&msg_info, first_client_info);
    continue;
}
else { 
    // 일반 메시지 처리
    sprintf(to_msg, "[%s]%s", msg_info.from, pArray[1]);
    msg_info.msg = to_msg;
    msg_info.len = strlen(to_msg);
    sprintf(strBuff, "msg : [%s->%s] %s\n", msg_info.from, msg_info.to, pArray[1]);
    log_file(strBuff);
}

send_msg(&msg_info, first_client_info);
    }

    close(client_info->fd);

    sprintf(strBuff,"Disconnect ID:%s (ip:%s,fd:%d,sockcnt:%d)\n",client_info->id,client_info->ip,client_info->fd,clnt_cnt-1);
    log_file(strBuff);

    pthread_mutex_lock(&mutx);
    clnt_cnt--;
    client_info->fd = -1;
    pthread_mutex_unlock(&mutx);

    return 0;
}


void send_msg(MSG_INFO * msg_info, CLIENT_INFO * first_client_info)
{
    int i=0;
    
    // 메시지가 날씨 데이터인지 확인 (길이로 판단)
    int is_weather_data = (strstr(msg_info->msg, "날씨정보") != NULL) || 
                         (strstr(msg_info->msg, "미세먼지") != NULL) ||
                        (strstr(msg_info->msg, "일정") != NULL);
    
    // 날씨 데이터는 한 번에 전송하도록 처리
    if (is_weather_data) {
        // 버퍼 오버플로우 방지
        int max_len = msg_info->len > 1024 ? 1024 : msg_info->len;
        
        // 메시지 종료 마커 추가
        char *complete_msg = (char *)malloc(max_len + 10); // 추가 공간 확보
        strcpy(complete_msg, msg_info->msg);
        strcat(complete_msg, "##END##"); // 특별한 종료 마커 추가
        
        // 특정 사용자에게 전송
        if (strcmp(msg_info->to, "ALLMSG") != 0 && strcmp(msg_info->to, "IDLIST") != 0) {
            for(i=0; i<MAX_CLNT; i++) {
                if((first_client_info+i)->fd != -1) {
                    if(!strcmp(msg_info->to, (first_client_info+i)->id)) {
                        // 한 번에 전송
                        write((first_client_info+i)->fd, complete_msg, strlen(complete_msg));
                        char log_buf[100];
                        sprintf(log_buf, "날씨 데이터 전송 완료: %d 바이트 (수신자: %s, 종료 마커 포함)\n", 
                                strlen(complete_msg), msg_info->to);
                        log_file(log_buf);
                    }
                }
            }
        } else if(!strcmp(msg_info->to, "ALLMSG")) {
            // 모든 사용자에게 전송
            for(i=0; i<MAX_CLNT; i++)
                if((first_client_info+i)->fd != -1)
                    write((first_client_info+i)->fd, complete_msg, strlen(complete_msg));
        }
        
        free(complete_msg); // 동적 할당 메모리 해제

    } else {
        // 기존 로직 유지 (날씨 데이터가 아닌 일반 메시지)
        if(!strcmp(msg_info->to, "ALLMSG")) {
            for(i=0; i<MAX_CLNT; i++)
                if((first_client_info+i)->fd != -1)   
                    write((first_client_info+i)->fd, msg_info->msg, msg_info->len);
        }
        else if(!strcmp(msg_info->to, "IDLIST")) {
            char* idlist = (char *)malloc(ID_SIZE * MAX_CLNT);
            msg_info->msg[strlen(msg_info->msg) - 1] = '\0';
            strcpy(idlist,msg_info->msg);

            for(i=0; i<MAX_CLNT; i++) {
                if((first_client_info+i)->fd != -1) {
                    strcat(idlist,(first_client_info+i)->id);
                    strcat(idlist," ");
                }
            }
            strcat(idlist,"\n");
            write(msg_info->fd, idlist, strlen(idlist));
            free(idlist);
        }
        else if(!strcmp(msg_info->to, "GETTIME")) {
            sleep(1);
            getlocaltime(msg_info->msg);
            write(msg_info->fd, msg_info->msg, strlen(msg_info->msg));
        }
        else {
            for(i=0; i<MAX_CLNT; i++)
                if((first_client_info+i)->fd != -1)   
                    if(!strcmp(msg_info->to, (first_client_info+i)->id))
                        write((first_client_info+i)->fd, msg_info->msg, msg_info->len);
        }
    }
}

void error_handling(char *msg)
{
      fputs(msg, stderr);
      fputc('\n', stderr);
      exit(1);
}

void log_file(char * msgstr)
{
      fputs(msgstr,stdout);
}
void  getlocaltime(char * buf)
{
   struct tm *t;
   time_t tt;
   char wday[7][4] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
   tt = time(NULL);
   if(errno == EFAULT)
      perror("time()");
   t = localtime(&tt);
   sprintf(buf,"[GETTIME]%02d.%02d.%02d %02d:%02d:%02d %s\n",t->tm_year+1900-2000,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,wday[t->tm_wday]);
   return;
}

// 날짜가 지난 일정 정리 - 오늘과 내일 일정만 유지
void cleanup_expired_schedules() {
    FILE *old_file = fopen("schedule.txt", "r");
    if (old_file == NULL) {
        log_file("일정 파일이 없습니다. 정리 작업 생략\n");
        return;
    }
    
    // 임시 파일 생성
    FILE *new_file = fopen("schedule_temp.txt", "w");
    if (new_file == NULL) {
        fclose(old_file);
        log_file("임시 파일 생성 실패. 정리 작업 생략\n");
        return;
    }
    
    // 오늘 날짜 가져오기
    time_t now = time(NULL);
    struct tm *today = localtime(&now);
    int today_year = today->tm_year + 1900;
    int today_month = today->tm_mon + 1;
    int today_day = today->tm_mday;
    
    // 내일 날짜 계산
    struct tm tomorrow = *today;
    tomorrow.tm_mday += 1;
    mktime(&tomorrow); // 날짜 정규화
    int tomorrow_year = tomorrow.tm_year + 1900;
    int tomorrow_month = tomorrow.tm_mon + 1;
    int tomorrow_day = tomorrow.tm_mday;
    
    char line[256];
    int kept_count = 0;
    int removed_count = 0;
    
    // 한 줄씩 읽어서 처리
    while (fgets(line, sizeof(line), old_file)) {
        // 개행 문자 제거
        line[strcspn(line, "\n")] = 0;
        
        // 날짜 부분 추출 (포맷: 제목:설명:날짜:시작시간:중요도)
        char *title_end = strchr(line, ':');
        if (title_end == NULL) continue;
        
        char *desc_end = strchr(title_end + 1, ':');
        if (desc_end == NULL) continue;
        
        char *date_start = desc_end + 1;
        char *date_end = strchr(date_start, ':');
        if (date_end == NULL) continue;
        
        int date_len = date_end - date_start;
        char date_str[20];
        if (date_len >= sizeof(date_str)) date_len = sizeof(date_str) - 1;
        strncpy(date_str, date_start, date_len);
        date_str[date_len] = '\0';
        
        // 날짜 파싱
        int year, month, day;
        if (sscanf(date_str, "%d/%d/%d", &year, &month, &day) == 3) {
            // 오늘이나 내일 일정인지 확인
            if ((year == today_year && month == today_month && day == today_day) ||
                (year == tomorrow_year && month == tomorrow_month && day == tomorrow_day)) {
                // 오늘이나 내일 일정은 유지
                fprintf(new_file, "%s\n", line);
                kept_count++;
            } else {
                // 다른 날짜 일정은 제거
                removed_count++;
            }
        } else {
            // 날짜 파싱 실패 시 일단 유지
            fprintf(new_file, "%s\n", line);
            kept_count++;
        }
    }
    
    // 파일 닫기
    fclose(old_file);
    fclose(new_file);
    
    // 임시 파일을 원래 파일로 이동
    remove("schedule.txt");
    rename("schedule_temp.txt", "schedule.txt");
    
    char log_message[100];
    sprintf(log_message, "일정 정리 완료: %d개 유지 (오늘/내일 일정), %d개 제거됨\n", kept_count, removed_count);
    log_file(log_message);
}