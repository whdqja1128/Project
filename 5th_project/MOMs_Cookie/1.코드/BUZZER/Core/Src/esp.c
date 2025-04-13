//서울기술교육센터 AIOT & Embedded System
//2024-04-16 By KSH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "esp.h"

static char ip_addr[16];
char response[MAX_ESP_RX_BUFFER]; // static 제거 - 외부에서 접근 가능하도록 변경
//==================uart2=========================
extern UART_HandleTypeDef huart2;  // 외부 정의 참조로 변경
volatile unsigned char rx2Flag = 0;
volatile char rx2Data[50];
uint8_t cdata;

//==================uart6=========================
extern volatile unsigned char rx2Flag;
extern volatile char rx2Data[50];
extern uint8_t cdata;
//extern UART_HandleTypeDef huart2;
static uint8_t data;
//static cb_data_t cb_data;
cb_data_t cb_data; // 이미 static 제거됨
extern UART_HandleTypeDef huart6;  // 외부 정의 참조로 변경

// 연결 상태 관련 변수 추가
static uint32_t last_connection_check = 0;  // 마지막 연결 확인 시간
static uint32_t connection_established_time = 0; // 연결 설정 시간
static bool connection_stable = false;      // 안정적 연결 여부

// 서버로부터 명령을 수신하고 처리하기 위한 콜백 함수 포인터
static void (*server_command_callback)(char *cmd) = NULL;

// 서버 명령 콜백 함수 등록
void register_server_command_callback(void (*callback)(char *cmd))
{
    server_command_callback = callback;
}

// 개선된 esp_at_command 함수 - 응답 처리 및 타이밍 최적화
int esp_at_command(uint8_t *cmd, uint8_t *resp, uint16_t *length, int16_t time_out)
{
    *length = 0;
    memset(resp, 0x00, MAX_UART_RX_BUFFER);
    memset(&cb_data, 0x00, sizeof(cb_data_t));

    // 명령 전송 전 버퍼 비우기
    HAL_Delay(50); // 100ms에서 50ms로 감소

    if(HAL_UART_Transmit(&huart6, cmd, strlen((char *)cmd), 100) != HAL_OK) {
        return -1;
    }

    // 최소 대기 시간 조정
    int min_wait = 200; // 300ms에서 200ms로 감소
    if(time_out < min_wait) time_out = min_wait;

    int elapsed = 0;
    int check_interval = 20; // 30ms에서 20ms로 감소 - 더 자주 확인

    while(elapsed < time_out)
    {
        HAL_Delay(check_interval);
        elapsed += check_interval;

        // 응답 버퍼 초과 확인
        if(cb_data.length >= MAX_ESP_RX_BUFFER)
            break; // 타임아웃이 아닌 버퍼 초과로 처리

        // 에러 응답 즉시 처리
        if(strstr((char *)cb_data.buf, "ERROR") != NULL) {
            memcpy(resp, cb_data.buf, cb_data.length);
            *length = cb_data.length;
            return -3;
        }

        // 성공 응답 즉시 처리
        if(strstr((char *)cb_data.buf, "OK") != NULL ||
           strstr((char *)cb_data.buf, ">") != NULL)
        {
            memcpy(resp, cb_data.buf, cb_data.length);
            *length = cb_data.length;
            return 0;
        }
    }

    // 타임아웃 처리 개선
    if(cb_data.length > 0) {
        memcpy(resp, cb_data.buf, cb_data.length);
        *length = cb_data.length;

        // 불완전한 응답이지만 성공으로 볼 수 있는 경우 처리
        if(strstr((char *)cb_data.buf, "CONNECT") != NULL) {
            return 0;
        }
    }

    return -4; // 타임아웃
}

// ESP 모듈 리셋 함수
int esp_reset(void)
{
    uint16_t length = 0;
    printf("ESP 모듈 리셋 중...\r\n");

    if(esp_at_command((uint8_t *)"AT+RST\r\n", (uint8_t *)response, &length, 1000) != 0)
    {
       printf("ESP 리셋 명령 실패\r\n");
       return -1;
    }

    // 리셋 후 추가 대기 시간
    HAL_Delay(1000);

    return esp_at_command((uint8_t *)"AT\r\n", (uint8_t *)response, &length, 1000);
}

// IP 주소 얻기 함수
static int esp_get_ip_addr(uint8_t is_debug)
{
    if(strlen(ip_addr) != 0)
    {
        if(strcmp(ip_addr, "0.0.0.0") == 0)
            return -1;
    }
    else
    {
        uint16_t length;
        if(esp_at_command((uint8_t *)"AT+CIPSTA?\r\n", (uint8_t *)response, &length, 1000) != 0)
            printf("ip_state command fail\r\n");
        else
        {
            char *line = strtok(response, "\r\n");

            if(is_debug)
            {
                for(int i = 0 ; i < length ; i++)
                    printf("%c", response[i]);
            }

            while(line != NULL)
            {
                if(strstr(line, "ip:") != NULL)
                {
                    char *ip;

                    strtok(line, "\"");
                    ip = strtok(NULL, "\"");
                    if(strcmp(ip, "0.0.0.0") != 0)
                    {
                        memset(ip_addr, 0x00, sizeof(ip_addr));
                        memcpy(ip_addr, ip, strlen(ip));
                        return 0;
                    }
                }
                line = strtok(NULL, "\r\n");
            }
        }

        return -1;
    }

    return 0;
}



// IP 상태 확인 함수
void ip_state_func()
{
    if(esp_get_ip_addr(1) != 0)
        printf("ip address empty\r\n");
    else
        printf("ip address = %s\r\n", ip_addr);
}

// 데이터 전송 함수 개선
int esp_send_data(char *data)
{
    char at_cmd[MAX_ESP_COMMAND_LEN] = {0, };
    uint16_t length = 0;

    // 서버 연결 상태 확인
    if (!check_server_connection()) {
        return -1;
    }

    // 전송 명령 구성
    int data_length = strlen(data);
    memset(at_cmd, 0, sizeof(at_cmd));
    sprintf(at_cmd, "AT+CIPSEND=%d\r\n", data_length);

    // 전송 명령 전송
    if(esp_at_command((uint8_t *)at_cmd, (uint8_t *)response, &length, 2000) != 0 ||
       strstr((char *)response, ">") == NULL) {
        return -2;
    }

    // 전송 데이터 버퍼 초기화
    memset(response, 0, MAX_ESP_RX_BUFFER);
    cb_data.length = 0;

    // 데이터 전송
    if(HAL_UART_Transmit(&huart6, (uint8_t *)data, data_length, 1000) != HAL_OK) {
        return -3;
    }

    // 전송 성공 응답 확인
    int timeout = 2000;
    bool send_ok = false;

    while(timeout > 0) {
        if(strstr((char *)cb_data.buf, "SEND OK") != NULL) {
            send_ok = true;
            break;
        }
        HAL_Delay(20);
        timeout -= 20;
    }

    if(!send_ok) {
        return -5;
    }

    return 0;
}

// 서버 이벤트 처리 함수
void esp_event(char *recvBuf)
{
    // 중복 로그인 메시지를 체크하고 무시
    if(strstr(recvBuf, "Already logged") != NULL) {
        return; // 중복 로그인 메시지는 무시하고 출력하지 않음
    }

    // 서버로부터 명령 수신 처리 추가
    if(strstr(recvBuf, "+IPD,") != NULL) {
        char *data_start = strstr(recvBuf, ":");

        if(data_start != NULL && server_command_callback != NULL) {
            data_start++; // 콜론 다음 문자부터 실제 데이터

            // NULL 종료 문자 추가
            char cmd_buffer[100] = {0};
            strncpy(cmd_buffer, data_start, sizeof(cmd_buffer)-1);

            // 불필요한 개행 문자 제거
            char *newline = strchr(cmd_buffer, '\r');
            if (newline) *newline = '\0';
            newline = strchr(cmd_buffer, '\n');
            if (newline) *newline = '\0';

            // 양쪽 공백 제거
            char *start = cmd_buffer;
            char *end = cmd_buffer + strlen(cmd_buffer) - 1;
            while (*start && (*start == ' ' || *start == '\t')) start++;
            while (end > start && (*end == ' ' || *end == '\t')) *end-- = '\0';

            // 서버 명령 콜백 직접 호출 (로그 출력 제거)
            server_command_callback(start);
        }
    }
}

// UART 초기화 함수
void drv_uart_init(void)
{
    if(HAL_UART_Receive_IT(&huart6, &data, 1) != HAL_OK)
        printf("수신 인터럽트 초기화 실패\r\n");
}

// UART 수신 버퍼 처리 함수
int drv_uart_rx_buffer(uint8_t *data_p, uint16_t length)
{
    uint16_t rx_len = 0;
    uint8_t ch = 0; // 경고: 사용하지 않는 변수이지만 코드 일관성을 위해 유지

    // uart2로부터 수신
    do
    {
        if(rx2Flag == 1)
        {
            memcpy(data_p, (void*)rx2Data, strlen((char *)rx2Data)); // 타입 캐스팅 추가
            rx_len = strlen((char *)rx2Data);
            rx2Flag = 0;
            break;
        }

        HAL_Delay(10);
    } while(rx_len < length);

    return rx_len;
}

// UART 전송 버퍼 처리 함수
int drv_uart_tx_buffer(uint8_t *data_p, uint16_t length)
{
    HAL_UART_Transmit(&huart2, data_p, length, 1000);

    return length;
}

// UART 수신 완료 콜백 함수
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t i;

    if(huart == &huart2)
    {
        for(i=0; rx2Data[i] != 0; i++);
        rx2Data[i] = cdata;
        rx2Data[i+1] = 0;

        if(cdata == '\r' || i >= 48)
        {
            rx2Flag = 1;
            memset((void*)rx2Data, 0, sizeof(rx2Data)); // 타입 캐스팅 추가
        }

        HAL_UART_Receive_IT(&huart2, &cdata, 1);
    }
    else if(huart == &huart6)
    {
        if(cb_data.length < MAX_ESP_RX_BUFFER)
        {
            cb_data.buf[cb_data.length++] = data;
            // 서버 이벤트 처리를 위한 버퍼 체크
            if (data == '\n' && cb_data.length > 2) {
                char temp_buf[MAX_ESP_RX_BUFFER] = {0};
                strncpy(temp_buf, (char*)cb_data.buf, cb_data.length);
                esp_event(temp_buf);
            }
        }

        HAL_UART_Receive_IT(&huart6, &data, 1);
    }
}

// IP 주소 요청 함수
int request_ip_addr(uint8_t is_debug)
{
    uint16_t length = 0;

    if(esp_at_command((uint8_t *)"AT+CIFSR\r\n", (uint8_t *)response, &length, 1000) != 0)
        printf("request ip_addr command fail\r\n");
    else
    {
        char *line = strtok(response, "\r\n");

        if(is_debug)
        {
            for(int i = 0 ; i < length ; i++)
                printf("%c", response[i]);
        }

        while(line != NULL)
        {
            if(strstr(line, "CIFSR:STAIP") != NULL)
            {
                char *ip;

                strtok(line, "\"");
                ip = strtok(NULL, "\"");
                if(strcmp(ip, "0.0.0.0") != 0)
                {
                    memset(ip_addr, 0x00, sizeof(ip_addr));
                    memcpy(ip_addr, ip, strlen(ip));
                    return 0;
                }
            }
            line = strtok(NULL, "\r\n");
        }
    }

    return -1;
}

// 서버 연결 상태 확인 함수
bool check_server_connection(void)
{
    uint16_t length = 0;
    static uint32_t last_successful_check = 0;
    static bool last_connection_state = false;

    // 캐싱 시간 관리
    uint32_t current_time = HAL_GetTick();
    if (current_time - last_connection_check < 15000 && connection_established_time > 0) {
        return connection_stable;
    }

    // 마지막 성공 확인으로부터 짧은 시간 내라면 재확인 건너뜀
    if (connection_stable && current_time - last_successful_check < 45000) {
        last_connection_check = current_time;
        return true;
    }

    // 1. CIPSTATUS 명령으로 확인
    if(esp_at_command((uint8_t *)"AT+CIPSTATUS\r\n", (uint8_t *)response, &length, 1000) != 0) {
        // 명령 자체가 실패했다면, 간단히 AT 명령으로 모듈 응답 확인
        if(esp_at_command((uint8_t *)"AT\r\n", (uint8_t *)response, &length, 500) != 0) {
            // 상태 변경 감지
            if (last_connection_state) {
                printf("Disconnected\r\n");
                last_connection_state = false;
            }

            connection_stable = false;
            last_connection_check = current_time;
            return false;
        }

        // AT는 응답하지만 CIPSTATUS가 실패하면 연결이 끊어졌을 가능성 높음
        if (last_connection_state) {
            printf("Disconnected\r\n");
            last_connection_state = false;
        }

        connection_stable = false;
        last_connection_check = current_time;
        return false;
    }

    // 2. STATUS 응답 확인
    if(strstr((char *)response, "STATUS:3") != NULL ||  // 연결됨, 데이터 전송 가능
       strstr((char *)response, "STATUS:4") != NULL ||  // 연결 해제됨, 하지만 재연결 가능
       strstr((char *)response, "STATUS:2") != NULL ||  // 연결 중
       strstr((char *)response, "STATUS:5") != NULL) {  // 연결은 끊겼지만 WiFi 연결은 유지

        // 연결이 성공적으로 확인됨
        if (!connection_stable) {
            connection_established_time = current_time;
        }

        // 이전에 연결 안되어 있다가 지금 연결되었으면 New Connected 출력
        if (!last_connection_state) {
            printf("New Connected\r\n");
            last_connection_state = true;
        }

        connection_stable = true;
        last_connection_check = current_time;
        last_successful_check = current_time;
        return true;
    }

    // 상태 코드가 위의 조건과 일치하지 않으면 연결 실패
    if (last_connection_state) {
        printf("Disconnected\r\n");
        last_connection_state = false;
    }

    connection_stable = false;
    last_connection_check = current_time;
    return false;
}

// AP 연결 함수
void ap_conn_func(char *ssid, char *passwd)
{
  uint16_t length = 0;
  char at_cmd[MAX_ESP_COMMAND_LEN] = {0, };
  static bool ap_connected = false;

  if(ssid == NULL || passwd == NULL) {
      return;
  }

  // 이미 연결되어 있는지 확인
  if(ap_connected) {
      return;
  }

  // ESP 모듈 리셋 수행
  esp_reset();
  HAL_Delay(1000);

  // WiFi 모드 설정: 1 = Station 모드
  memset(at_cmd, 0x00, sizeof(at_cmd));
  esp_at_command((uint8_t *)"AT+CWMODE=1\r\n", (uint8_t *)response, &length, 1000);
  HAL_Delay(500);

  // 자동 연결 설정: 1 = 전원이 켜질 때 자동으로 AP에 연결
  esp_at_command((uint8_t *)"AT+CWAUTOCONN=1\r\n", (uint8_t *)response, &length, 1000);
  HAL_Delay(300);

  // AP에 연결 시도 (한 번만)
  memset(at_cmd, 0x00, sizeof(at_cmd));
  sprintf(at_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, passwd); // 패스워드 파라미터 추가
  printf("와이파이 연결 중...\r\n");

  // 연결 한 번만 시도
  if(esp_at_command((uint8_t *)at_cmd, (uint8_t *)response, &length, 10000) == 0) {
    printf("와이파이 연결 성공!\r\n");
    ap_connected = true;
  } else {
    printf("와이파이 연결 실패\r\n");
  }
}

int esp_client_conn()
{
    char at_cmd[MAX_ESP_COMMAND_LEN] = {0, };
    uint16_t length = 0;
    static uint32_t last_full_conn_time = 0;
    uint32_t current_time = HAL_GetTick();

    // 매번 다른 임의의 고유 ID 생성으로 중복 로그인 방지
    char device_id[30] = {0};
    sprintf(device_id, "USR_STM32", (unsigned long)(current_time % 10000));

    // 최근 연결 시도 검사 - 너무 빈번한 시도 방지
    if (current_time - last_full_conn_time < 15000 && last_full_conn_time > 0) {
        return -1;
    }

    last_full_conn_time = current_time;

    // 기존 연결 종료 명령
    esp_at_command((uint8_t *)"AT+CIPCLOSE\r\n", (uint8_t *)response, &length, 500);
    HAL_Delay(500); // 충분한 대기 시간 추가

    // DHCP 활성화
    esp_at_command((uint8_t *)"AT+CWDHCP=1,1\r\n", (uint8_t *)response, &length, 1000);
    HAL_Delay(100);

    // 단일 연결 모드 설정
    esp_at_command((uint8_t *)"AT+CIPMUX=0\r\n", (uint8_t *)response, &length, 1000);
    HAL_Delay(200);

    // TCP 연결 명령
    memset(at_cmd, 0, sizeof(at_cmd));
    sprintf(at_cmd,"AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", DST_IP, DST_PORT);

    // 연결 명령 실행 및 결과 확인
    if(esp_at_command((uint8_t *)at_cmd, (uint8_t *)response, &length, 8000) == 0) {
        if(strstr((char *)response, "CONNECT") != NULL ||
           strstr((char *)response, "ALREADY CONNECTED") != NULL) {

            // 로그인 메시지 전송 - 고유 ID 사용하여 중복 로그인 방지
            memset(at_cmd, 0, sizeof(at_cmd));
            sprintf(at_cmd, "AT+CIPSEND=%d\r\n", strlen(device_id));

            if(esp_at_command((uint8_t *)at_cmd, (uint8_t *)response, &length, 2000) != 0 ||
               strstr((char *)response, ">") == NULL) {
                return -2;
            }

            // 로그인 데이터 전송
            memset(response, 0, MAX_ESP_RX_BUFFER);
            cb_data.length = 0;

            if(HAL_UART_Transmit(&huart6, (uint8_t *)device_id, strlen(device_id), 1000) != HAL_OK) {
                return -3;
            }

            // 응답 대기
            int timeout = 3000;
            bool send_ok = false;

            while(timeout > 0) {
                if(strstr((char *)cb_data.buf, "SEND OK") != NULL) {
                    send_ok = true;
                    break;
                }
                HAL_Delay(20);
                timeout -= 20;
            }

            if(!send_ok) {
                return -4;
            }

            // 연결 상태 변수 업데이트
            connection_established_time = HAL_GetTick();
            connection_stable = true;
            last_connection_check = connection_established_time;

            // 연결 성공 메시지 출력
            printf("New Connected\r\n");

            return 0;  // 성공적으로 연결됨
        }
    }

    connection_stable = false;
    return -1;
}

// ESP 드라이버 초기화 함수
int drv_esp_init(void)
{
    printf("ESP 드라이버 초기화 중...\r\n");

    // UART 통신 설정
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 38400;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;

    if(HAL_UART_Init(&huart6) != HAL_OK) {
        printf("USART6 초기화 실패\r\n");
        return -1;
    }

    // 수신 버퍼 초기화
    memset(ip_addr, 0x00, sizeof(ip_addr));
    HAL_UART_Receive_IT(&huart6, &data, 1);

    // 상태 변수 초기화
    connection_established_time = 0;
    connection_stable = false;
    last_connection_check = 0;

    // ESP 모듈 리셋 - 2번 시도
    printf("ESP 모듈 리셋 중...\r\n");
    for(int i = 0; i < 2; i++) {
        if(esp_reset() == 0) {
            printf("ESP 모듈 리셋 성공\r\n");

            // 추가 설정: 에코 끄기
            uint16_t length = 0;
            esp_at_command((uint8_t *)"ATE0\r\n", (uint8_t *)response, &length, 500);

                        // 안정화를 위한 대기
                        HAL_Delay(1000);
                        return 0;
                    }
                    printf("ESP 모듈 리셋 시도 %d/2 실패...\r\n", i+1);
                    HAL_Delay(1000);
                }

                printf("ESP 모듈 초기화 실패\r\n");
                return -1;
            }

            // 리셋 함수
            void reset_func()
            {
                printf("esp reset... ");
                if(esp_reset() == 0)
                    printf("OK\r\n");
                else
                    printf("fail\r\n");
            }

            // 버전 확인 함수
            void version_func()
            {
                uint16_t length = 0;
                printf("esp firmware version\r\n");
                if(esp_at_command((uint8_t *)"AT+GMR\r\n", (uint8_t *)response, &length, 1000) != 0)
                    printf("ap scan command fail\r\n");
                else
                {
                    for(int i = 0 ; i < length ; i++)
                        printf("%c", response[i]);
                }
            }

            // ESP 테스트 명령 처리 함수
            int drv_esp_test_command(void)
            {
                char command[MAX_UART_COMMAND_LEN];
                uint16_t length = 0;

                while(1)
                {
                    drv_uart_tx_buffer((uint8_t *)"esp>", 4);

                    memset(command, 0x00, sizeof(command));
                    if(drv_uart_rx_buffer((uint8_t *)command, MAX_UART_COMMAND_LEN) == 0)
            			continue;

                    if(strcmp(command, "help") == 0)
                    {
                        printf("============================================================\r\n");
                        printf("* help                    : help\r\n");
                        printf("* quit                    : esp test exit\r\n");
                        printf("* reset                   : esp restart\r\n");
                        printf("* version                 : esp firmware version\r\n");
                        printf("* ap_scan                 : scan ap list\r\n");
                        printf("* ap_conn <ssid> <passwd> : connect ap & obtain ip addr\r\n");
                        printf("* ap_disconnect           : disconnect ap\r\n");
                        printf("* ip_state                : display ip addr\r\n");
                        printf("* request_ip              : obtain ip address\r\n");
                        printf("* AT+<XXXX>               : AT COMMAND\r\n");
                        printf("*\r\n");
                        printf("* More <AT COMMAND> information is available on the following website\r\n");
                        printf("*  - https://docs.espressif.com/projects/esp-at/en/latest/AT_Command_Set\r\n");
                        printf("============================================================\r\n");
                    }
                    else if(strcmp(command, "quit") == 0)
                    {
                        printf("esp test exit\r\n");
                        break;
                    }
                    else if(strcmp(command, "reset") == 0)
                    {
                    		reset_func();
                    }
                    else if(strcmp(command, "version") == 0)
                    {
                    		version_func();
                    }
                    else if(strcmp(command, "ap_scan") == 0)
                    {
                        printf("ap scan...\r\n");
                        if(esp_at_command((uint8_t *)"AT+CWLAP\r\n", (uint8_t *)response, &length, 5000) != 0)
                            printf("ap scan command fail\r\n");
                        else
                        {
                            for(int i = 0 ; i < length ; i++)
                                printf("%c", response[i]);
                        }
                    }
                    else if(strncmp(command, "ap_conn", strlen("ap_conn")) == 0)
                    {
                    		ap_conn_func(SSID,PASS);
                    }
                    else if(strcmp(command, "ap_disconnect") == 0)
                    {
                        if(esp_at_command((uint8_t *)"AT+CWQAP\r\n", (uint8_t *)response, &length, 1000) != 0)
                            printf("ap connected info command fail\r\n");
                        else
                        {
                            for(int i = 0 ; i < length ; i++)
                                printf("%c", response[i]);

                            memset(ip_addr, 0x00, sizeof(ip_addr));
                        }
                    }
                    else if(strcmp(command, "ip_state") == 0)
                    {
                    		ip_state_func();
                    }
                    else if(strcmp(command, "request_ip") == 0)
                    {
                        request_ip_addr(1);
                    }
                    else if(strncmp(command, "AT+", 3) == 0)
                    {
                        uint8_t at_cmd[MAX_ESP_COMMAND_LEN+3];

                        memset(at_cmd, 0x00, sizeof(at_cmd));
                        sprintf((char *)at_cmd, "%s\r\n", command);
                        if(esp_at_command(at_cmd, (uint8_t *)response, &length, 1000) != 0)
                            printf("AT+ command fail\r\n");
                        else
                        {
                            for(int i = 0 ; i < length ; i++)
                                printf("%c", response[i]);
                        }
                    }
                    else
                    {
                        printf("invalid command : %s\r\n", command);
                    }
                }

                return 0;
            }

            int _write(int file, char *ptr, int len)
            {
            	HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, 1000);
            	return len;
            }
