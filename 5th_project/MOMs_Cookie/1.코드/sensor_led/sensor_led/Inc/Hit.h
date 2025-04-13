/**
  ******************************************************************************
  * @file           : heat.h
  * @brief          : Header for heat.c file
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HIT_H
#define __HIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
float Convert_To_Temperature(uint16_t adc_val);
uint16_t HEAT_Read(void);
float HEAT_readTemperature(void);
void Set_Pin_ANALOG_HEAT(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* __HEAT_H */
