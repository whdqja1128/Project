
#ifndef INC_WATER_H_
#define INC_WATER_H_

#include "stm32f4xx_hal.h"

#define WATER_PORT GPIOA
#define WATER_PIN GPIO_PIN_4

typedef struct {
    uint16_t water_value;
} WATER_Typedef;

WATER_Typedef WATER_readData();
extern ADC_HandleTypeDef hadc1;

uint16_t WATER_Read (void);
void Set_Pin_ANALOG_WATER (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#endif /* INC_WATER_H_ */
