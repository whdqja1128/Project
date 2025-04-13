/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "water.h"
#include "esp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// 알람 상태를 위한 열거형 정의
typedef enum {
  ALARM_STATE_IDLE,      // 대기 상태
  ALARM_STATE_ACTIVE,    // 알람 활성화 상태
  ALARM_STATE_WAITING    // 재시작 대기 상태
} AlarmState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 부저 제어를 위한 매크로 정의
#define BUZZER_ON GPIO_PIN_SET   // 부저 켜기
#define BUZZER_OFF GPIO_PIN_RESET // 부저 끄기

// 서버 연결 및 데이터 전송 관련 매크로
#define SERVER_RECONNECT_TIME 60000  // 서버 재연결 시도 간격 (60초)
#define CONN_RETRY_MAX 3             // 연결 재시도 최대 횟수

// 알람 관련 매크로
#define MAX_ALARM_COUNT 3    // 최대 알람 횟수 3개로 수정
#define ARR_CNT 7
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
// 알람 상태 변수들
AlarmState current_alarm_state = ALARM_STATE_IDLE;
bool buzzer_state = false;         // 현재 부저 상태
bool alarm_active = false;         // 알람 활성화 상태
uint8_t current_alarm_index = 0;   // 현재 실행 중인 알람 인덱스
uint32_t last_time_print = 0;      // 시간 출력을 위한 전역 변수

// 서버 연결 관련 변수들
bool server_connected = true;     // 서버 연결 상태
uint8_t conn_retry_count = 0;      // 연결 재시도 카운터
uint32_t last_attempt_time = 0;    // 마지막 연결 시도 시간
uint32_t last_conn_check_time = 0; // 마지막 서버 연결 확인 시간
uint8_t connection_errors = 0;     // 연결 오류 카운터

char recv_line[100];
uint8_t idx = 0;

// 다중 알람 시간 설정을 위한 구조체 및 배열
typedef struct {
    int hour;
    int minute;
    int second;
    bool active;
} AlarmTime;

// 의미 없는 지정 시간 삭제X
AlarmTime alarm_times[MAX_ALARM_COUNT] = {
    {20, 8, 0, true},
    {20, 9, 0, true},
    {20, 10, 0, true}
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM4_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void Buzzer_Control(bool state);              // 부저 제어 함수
bool Server_Connect(void);                    // 서버 연결 함수
void Reset_Connection(void);                  // 연결 초기화 함수
void handle_server_command(char *cmd);        // 서버 명령 처리 함수
void Set_Alarm_Time(uint8_t index, uint8_t hour, uint8_t minute); // 알람 시간 설정 함수 (인덱스 파라미터 추가)
void handle_esp_event(char *recvBuf);         // ESP 이벤트 처리 함수
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void poll_uart6_input() {
    uint8_t ch;
    while (HAL_UART_Receive(&huart6, &ch, 1, 10) == HAL_OK) {
        if (ch == '\n') {
            recv_line[idx] = '\0';
            printf("📥 STM32 수신: %s\r\n", recv_line);
            handle_esp_event(recv_line);
            idx = 0;
        } else if (idx < sizeof(recv_line) - 1) {
            recv_line[idx++] = ch;
        }
    }
}

void send_message_to_server(char *msg) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s\n", msg);
    HAL_UART_Transmit(&huart6, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

// 부저 제어 함수
void Buzzer_Control(bool state)
{
	// GPIO로 부저 제어
	    HAL_GPIO_WritePin(buzzer_GPIO_Port, buzzer_Pin, state ? BUZZER_ON : BUZZER_OFF);

	    // 상태가 변경될 때만 메시지 전송 (이미 같은 상태면 중복 메시지 방지)
	    if (buzzer_state != state) {
	        // 부저 상태 저장
	        buzzer_state = state;

	        // 서버에 상태 변경 메시지 전송
	        if (server_connected) {
	            if (state) {
	                // 부저가 켜질 때 개별 메시지 전송
	                printf("부저 ON 메시지 전송 시도\r\n");
	                send_message_to_server("[USR_BT]@SOUNDON\n");
	                HAL_Delay(50);
	                send_message_to_server("[USR_LIN]SOUNDON");
	                HAL_Delay(50);

	                // 디버깅용 로그
	                printf("부저 ON 메시지 전송 완료\r\n");
	            } else {
	                // 부저가 꺼질 때
	                printf("부저 OFF 메시지 전송 시도\r\n");
	                send_message_to_server("[STM32]SOUND@OFF");
	                HAL_Delay(50);

	                // 디버깅용 로그
	                printf("부저 OFF 메시지 전송 완료\r\n");
	            }
	        } else {
	            printf("서버 연결 안됨: 메시지 전송 실패\r\n");
	        }
	    }
	}

// 알람 시간 설정 함수 (인덱스 파라미터 추가)
void Set_Alarm_Time(uint8_t index, uint8_t hour, uint8_t minute)
{
//  // 인덱스 범위 확인
//  if (index >= MAX_ALARM_COUNT) {
//    printf("⚠️ 잘못된 알람 인덱스: %d (최대 %d)\r\n", index, MAX_ALARM_COUNT-1);
//    return;
//  }

  // 지정된 인덱스의 알람 설정
  alarm_times[index].hour = hour;
  alarm_times[index].minute = minute;
  alarm_times[index].second = 0;
  alarm_times[index].active = true;

  printf("🕒 알람 %d번 시간 설정: %02d시 %02d분\r\n", index, hour, minute);
}

// ESP 이벤트 처리 함수
void handle_esp_event(char *recvBuf)
{
  // 개행 문자 제거
  recvBuf[strcspn(recvBuf, "\r\n")] = 0;

  // 받은 메시지 전체 로깅
  //printf("수신 메시지 (raw): '%s'\r\n", recvBuf);

  // 연결 해제 확인
  if(strstr(recvBuf, "DISCONNECT") != NULL || strstr(recvBuf, "Disconnected") != NULL) {
    printf("ESP 이벤트: 서버 연결 해제\r\n");
    server_connected = false;
    printf("New Disconnected\r\n");
  }

  // 서버 명령어 처리
  handle_server_command(recvBuf);
}

// 서버 명령 처리 함수
void handle_server_command(char *cmd)
{
	// ALARM_SET 명령 처리 로직
	if(strstr(cmd, "ALARM_SET@") != NULL) {
	  printf("알람 설정 명령 수신: %s\r\n", cmd);

	  // [USR_AND]ALARM_SET@인덱스@시간@분 형식 파싱
	  int index, hour, minute;
	  if(sscanf(cmd, "[USR_AND]ALARM_SET@%d@%d@%d", &index, &hour, &minute) == 3) {
	    printf("알람 설정 요청: 인덱스=%d, 시간=%d시 %d분\r\n", index, hour, minute);

	    // 범위 확인 후 unsigned char로 변환하여 함수 호출
	    if(index >= 0 && index < MAX_ALARM_COUNT &&
	       hour >= 0 && hour <= 23 &&
	       minute >= 0 && minute <= 59) {
	      Set_Alarm_Time((uint8_t)index, (uint8_t)hour, (uint8_t)minute);
	    } else {
	      printf("알람 설정 범위 오류: 인덱스=%d, 시간=%d시 %d분\r\n", index, hour, minute);
	    }
	  } else {
	    printf("알람 설정 명령 형식 오류: %s\r\n", cmd);
	  }
	}

	// ALARM_DELETE 명령 처리 로직도 같은 형식으로 수정
	if(strstr(cmd, "ALARM_DELETE@") != NULL) {
	  printf("알람 삭제 명령 수신: %s\r\n", cmd);

	  // [USR_AND]ALARM_DELETE@인덱스 형식 파싱
	  int index;
	  if(sscanf(cmd, "[USR_AND]ALARM_DELETE@%d", &index) == 1) {
	    printf("알람 삭제 요청: 인덱스=%d\r\n", index);

	    // 해당 인덱스의 알람 비활성화
	    if(index >= 0 && index < MAX_ALARM_COUNT) {
	      alarm_times[index].active = false;
	      printf("알람 %d번 비활성화 완료\r\n", index);
	    } else {
	      printf("잘못된 알람 인덱스: %d\r\n", index);
	    }
	  } else {
	    printf("알람 삭제 명령 형식 오류: %s\r\n", cmd);
	  }
	}

  // 명령어 디버그 출력
  //printf("수신 명령어 분석: '%s'\r\n", cmd);

  // SOUND@OFF 명령 처리
  if(strstr(cmd, "SOUND@OFF") != NULL) {
    printf("부저 끄기 명령 수신: %s\r\n", cmd);

    // 부저 즉시 끄기
    Buzzer_Control(false);

    // 알람 상태 변수 초기화
    alarm_active = false;

    // 알람 상태를 IDLE로 변경
    current_alarm_state = ALARM_STATE_IDLE;

    // 시작 시간 변수 초기화를 위한 추가 코드
      printf("SOUND@OFF 명령으로 알람 종료\r\n");
  }

  // 시간 명령 처리
  if(strstr(cmd, "TIME@") != NULL) {
    // 시간 문자열 추출
    char *time_str = cmd + 14;

    //printf("🕒 서버 시간 수신: %s\r\n", time_str);

    // 시간 문자열 파싱 (HH:MM:SS 또는 HH-MM-SS 형식)
    int hours, minutes, seconds;
    if(sscanf(time_str, "%d-%d-%d", &hours, &minutes, &seconds) == 3) {

      printf("📅 파싱된 시간: %02d시-%02d분-%02d초\r\n", hours, minutes, seconds);

      // 현재 알람이 비활성화 상태일 때만 새 알람 확인
      if (current_alarm_state != ALARM_STATE_ACTIVE) {
        // 알람 배열을 순회하며 현재 시간과 일치하는 알람 찾기
        for (int i = 0; i < MAX_ALARM_COUNT; i++) {  // MAX_ALARM_COUNT(3)로 변경
          if (alarm_times[i].active &&
              hours == alarm_times[i].hour &&
              minutes == alarm_times[i].minute &&
              seconds < 5) { // 초는 5초 이내 허용

            printf("🚨 알람 시간 일치! 알람 %d번 시작 (%02d:%02d:%02d)\r\n",
                   i, alarm_times[i].hour, alarm_times[i].minute, alarm_times[i].second);

            // 현재 알람 인덱스 설정하고 부저 울림
            current_alarm_index = i;
            Buzzer_Control(true);
            alarm_active = true;
            current_alarm_state = ALARM_STATE_ACTIVE;

            break; // 한 번에 하나의 알람만 처리
          }
        }
      }
    }
  }
}

// 연결 초기화 함수
void Reset_Connection(void)
{
  server_connected = false;
  conn_retry_count = 0;
  connection_errors = 0;
  printf("연결 초기화됨\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  // UART 초기화 (printf 출력용)
  printf("\r\n\r\n===== 시스템 시작 =====\r\n");
  HAL_Delay(1000); // 시스템 안정화를 위한 지연

//  // 3개의 알람 시간 설정
//  Set_Alarm_Time(0, 19, 10);  // 첫 번째 알람
//  Set_Alarm_Time(1, 19, 11);  // 두 번째 알람
//  Set_Alarm_Time(2, 19, 00);  // 세 번째 알람

  // 서버 명령 콜백 함수 등록
  register_server_command_callback(handle_esp_event);
  printf("서버 명령 핸들러 등록 완료\r\n");

  // 초기화 변수 설정
  last_conn_check_time = HAL_GetTick();
  last_attempt_time = 0;
  current_alarm_state = ALARM_STATE_IDLE;

  printf("알람 시스템 초기화 완료 (총 %d개 알람 설정됨)\r\n", MAX_ALARM_COUNT);

  // 서버 연결 시도
  printf("시스템 초기화 완료, 서버 연결 준비 중...\r\n");
  HAL_Delay(2000);

  printf("메인 루프 시작\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 현재 시간 가져오기
    uint32_t current_time = HAL_GetTick();

    // 1초마다 현재 시간 요청
    if (current_time - last_time_print >= 1000) {
      if (server_connected) {
        // 시간 요청 문자열 전송
        char time_request[100];
        sprintf(time_request, "[USR_SQL]GET@TIME");
        send_message_to_server(time_request);
        //printf("🕒 서버에 시간 요청 전송\r\n");

        // 내부 시간 기반 알람 체크 (서버로부터 시간을 받기 전에 임시로 사용)
        if (1) {
          // 시스템 시간을 초 단위로 변환
          uint32_t sys_seconds = current_time / 1000;
          uint32_t sys_hours = (sys_seconds / 3600) % 24;
          uint32_t sys_minutes = (sys_seconds / 60) % 60;
          uint32_t sys_seconds_only = sys_seconds % 60;

          // 알람 배열을 순회하며 현재 시간과 일치하는 알람 찾기
          for (int i = 0; i < MAX_ALARM_COUNT; i++) {
            if (alarm_times[i].active &&
                sys_hours == alarm_times[i].hour &&
                sys_minutes == alarm_times[i].minute &&
                sys_seconds_only < 3) { // 초는 3초 이내 허용 (중복 알람 방지)

              printf("알람 시간 일치! 알람 %d번 시작 (내부시간: %02lu:%02lu:%02lu)\r\n",
                     i, sys_hours, sys_minutes, sys_seconds_only);

              // 현재 알람 인덱스 설정하고 부저 울림
              current_alarm_index = i;
              Buzzer_Control(true);
              alarm_active = true;
              current_alarm_state = ALARM_STATE_ACTIVE;

              break; // 한 번에 하나의 알람만 처리
            }
          }
        }
      }
      last_time_print = current_time;
    }

    poll_uart6_input();

    // 알람 상태가 활성화되어 있을 때 부저 상태 관리
    if (current_alarm_state == ALARM_STATE_ACTIVE) {
      // 상태 메시지를 한 번만 출력
      static bool status_printed = false;
      if (!status_printed) {
        printf("알람 활성화 상태\r\n");
        status_printed = true;
      }

      // 알람이 처음 활성화될 때 시작 시간 기록 (디버깅용으로 유지)
      static uint32_t alarm_start_time = 0;
      if (alarm_active && alarm_start_time == 0) {
        alarm_start_time = current_time;
        printf("알람 시작 시간: %lu\r\n", alarm_start_time);
      }

        // 알람 상태 초기화
        alarm_active = false;
        current_alarm_state = ALARM_STATE_IDLE;
        alarm_start_time = 0; // 다음 알람을 위해 초기화
        status_printed = false;

    }

    // 오류 임계값 초과 시 리셋
    if (connection_errors >= 5) {
      printf("연결 오류 임계값 초과, ESP 모듈 리셋\r\n");
      HAL_Delay(1000);
      Reset_Connection();
    }
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 10000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 84-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 20000-1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 38400;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, DHT11_Pin|buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DHT11_Pin buzzer_Pin */
  GPIO_InitStruct.Pin = DHT11_Pin|buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	  /* User can add his own implementation to report the HAL error return state */
	  __disable_irq();
	  while (1)
	  {
	  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	  /* User can add his own implementation to report the file name and line number,
	     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
