#include "macro.h"
#include "malloc.h"
#include "option.h"
#include "stm32f10x.h"

// Uart.c

#define Uart_Init Uart1_Init
#define Uart_Send_Byte Uart1_Send_Byte
#define Uart_Send_String Uart1_Send_String
#define Uart_Printf Uart1_Printf

extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern void Uart1_Send_String(char *pt);
extern void Uart1_Printf(char *fmt, ...);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);

// Led.c

extern void LED_Init(void);
extern void LED_Display(unsigned int num);
extern void LED_All_On(void);
extern void LED_All_Off(void);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern int Key_Wait_Key_Pressed(void);

// Timer.c

extern void TIM2_Out_Init(void);
extern void TIM2_Out_Freq_Generation(unsigned short freq);
extern void TIM2_Out_Stop(void);
extern void TIM2_PWM_Set_DutyCycle(uint16_t duty_cycle);
extern void TIM2_Set_Motor_Direction_And_Speed(uint8_t direction, uint16_t duty_cycle);

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// ADC.c

extern void Adc_Cds_Init(void);
extern void Adc_IN5_Init(void);
extern void Adc_Start(void);
extern void Adc_Stop(void);
extern int Adc_Get_Status(void);
extern int Adc_Get_Data(void);