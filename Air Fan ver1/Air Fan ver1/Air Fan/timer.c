#include "device_driver.h"

#define TIM2_FREQ (7200000)

void TIM2_Out_Init(void) {
  Macro_Set_Bit(RCC->APB1ENR, 0);
  Macro_Set_Bit(RCC->APB2ENR, 2);

  Macro_Write_Block(GPIOA->CRL, 0xff, 0x99, 8);

  Macro_Write_Block(TIM2->CCMR2, 0x7, 0x6, 4);
  Macro_Write_Block(TIM2->CCMR2, 0x7, 0x6, 12);

  TIM2->CCER |= (1 << 8);
  TIM2->CCER |= (1 << 12);
}

void TIM2_Out_Freq_Generation(unsigned short freq) {
  TIM2->PSC = (unsigned int)(TIMXCLK / (double)TIM2_FREQ + 0.5) - 1;
  TIM2->ARR = (unsigned int)((TIM2_FREQ) / (double)freq - 1);

  TIM2->CCR3 = TIM2->ARR / 2;
  TIM2->CCR4 = TIM2->ARR / 2;

  Macro_Set_Bit(TIM2->EGR, 0);

  TIM2->CR1 = (1 << 4) | (0 << 3) | (1 << 0);
}

void TIM2_Set_Motor_Direction_And_Speed(uint8_t direction, uint16_t duty_cycle) {
  if (direction == 0) {
    TIM2->CCR3 = (TIM2->ARR * duty_cycle) / 100;
    TIM2->CCR4 = 0;
  } else if (direction == 1) {
    TIM2->CCR3 = 0;
    TIM2->CCR4 = (TIM2->ARR * duty_cycle) / 100;
  } else {
    TIM2->CCR3 = 0;
    TIM2->CCR4 = 0;
  }
}

void TIM2_Out_Stop(void) {
  Macro_Clear_Bit(TIM2->CR1, 0);
  Macro_Clear_Bit(TIM2->DIER, 0);

  TIM2->CCER &= ~(1 << 8);
  TIM2->CCER &= ~(1 << 12);
}