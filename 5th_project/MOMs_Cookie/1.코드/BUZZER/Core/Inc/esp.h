#ifndef __ESP_H__
#define __ESP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>

// 매크로 정의 일관성 유지
#define MAX_ESP_COMMAND_LEN 128
#define MAX_ESP_RX_BUFFER 1024

// esp.c에서 MAX_UART_COMMAND_LEN과 MAX_UART_RX_BUFFER 사용 중이므로 호환을 위해 정의
#define MAX_UART_COMMAND_LEN MAX_ESP_COMMAND_LEN
#define MAX_UART_RX_BUFFER MAX_ESP_RX_BUFFER

#define SSID "embA"
#define PASS "embA1234"
#define LOGID "USR_STM32"
#define PASSWD "PASSWD"
#define DST_IP "10.10.141.73"
#define DST_PORT 5000

typedef struct callback_data
{
    uint8_t buf[MAX_ESP_RX_BUFFER];
    uint16_t length;
}cb_data_t;

extern cb_data_t cb_data;
extern char response[MAX_ESP_RX_BUFFER];

// 함수 프로토타입 선언
void register_server_command_callback(void (*callback)(char *cmd));
int esp_at_command(uint8_t *cmd, uint8_t *resp, uint16_t *length, int16_t time_out);
int esp_reset(void);
bool check_server_connection(void);
void ap_conn_func(char *ssid, char *passwd);
int esp_client_conn(void);
int drv_esp_init(void);
void reset_func(void);
void version_func(void);
int drv_esp_test_command(void);
void ip_state_func(void);
int esp_send_data(char *data);
void esp_event(char *recvBuf);
void drv_uart_init(void);
int drv_uart_rx_buffer(uint8_t *data_p, uint16_t length);
int drv_uart_tx_buffer(uint8_t *data_p, uint16_t length);
int request_ip_addr(uint8_t is_debug);

#ifdef __cplusplus
}
#endif

#endif /* __ESP_H__ */
