/* cds.h
 *
 *  Created on: Feb 28, 2025
 *      Author: IoT Main
 */

#ifndef INC_CDS_H_
#define INC_CDS_H_

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"

#define CDS_PORT GPIOA
#define CDS_PIN GPIO_PIN_5

typedef struct {
    uint16_t cds_value;  // CDS 센서의 ADC 값을 저장
} CDS_Typedef;

CDS_Typedef CDS_readData();
extern ADC_HandleTypeDef hadc1;

uint16_t CDS_Read(void);
void Set_Pin_Analog_CDS(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#endif /* INC_CDS_H_ */
