#include "device_driver.h"

volatile unsigned int autoMode = 0;
volatile unsigned int speed = 20;

static void Sys_Init(void) {
  Clock_Init();
  LED_Init();
  Uart_Init(115200);
  Key_Poll_Init();
  Adc_Cds_Init();
  TIM2_Out_Init();
  TIM2_Out_Freq_Generation(1000);
}

void mode_change(void) {
  if (Key_Get_Pressed() == 1) {
    autoMode = 1;
    Uart1_Printf("Auto Mode On\n");
    Key_Wait_Key_Released();
  } else if (Key_Get_Pressed() == 2) {
    autoMode = 2;
    speed += 20;
    if (speed > 100) {
      speed = 0;
    }
    Uart1_Printf("Motor spin constant speed : %d\n", speed);
    Key_Wait_Key_Released();
  }
}

void Auto_Mode() {
  Adc_Start();
  while (!Adc_Get_Status());
  int adc_value = Adc_Get_Data();

  int duty_cycle = (adc_value * 100) / 4095;

  TIM2_Set_Motor_Direction_And_Speed(0, duty_cycle);
}

int Get_Manual_Mode_Command(void) {
  char ch = 0;
  if (Macro_Check_Bit_Set(USART1->SR, 5)) {
    ch = USART1->DR;
  }
  if (ch == 'S' || ch == 's') {
    Uart1_Printf("Stop\n");
    return 1;
  } else if (ch == 'F' || ch == 'f') {
    Uart1_Printf("Forward\n");
    return 2;
  } else if (ch == 'R' || ch == 'r') {
    Uart1_Printf("Reverse\n");
    return 3;
  }

  return 0;
}

void Manual_Mode() {
  int i;
  static int Manual_Mode = 0;

  int command = Get_Manual_Mode_Command();
  if (command != 0) {
    Manual_Mode = command;
  }

  switch (Manual_Mode) {
    case 0:
      TIM2_Set_Motor_Direction_And_Speed(0, speed);
      break;
    case 1:
      TIM2_Set_Motor_Direction_And_Speed(2, 0);
      break;
    case 2:
      TIM2_Set_Motor_Direction_And_Speed(2, 0);
      for (i = 0; i < 1000000; i++);
      TIM2_Set_Motor_Direction_And_Speed(0, speed);
      break;
    case 3:
      TIM2_Set_Motor_Direction_And_Speed(2, 0);
      for (i = 0; i < 1000000; i++);
      TIM2_Set_Motor_Direction_And_Speed(1, speed);
      break;
  }
}

void Main(void) {
  Sys_Init();
  TIM2_Set_Motor_Direction_And_Speed(0, 50);
  Uart1_Printf("Air Fan On\n");
  autoMode = 1;

  for (;;) {
    mode_change();

    if (autoMode == 1) {
      Auto_Mode();
    } else if (autoMode == 2) {
      Manual_Mode();
    }
  }
}