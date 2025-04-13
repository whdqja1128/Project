/* cds.c
 *
 *  Created on: Feb 28, 2025
 *      Author: IoT Main
 */

#include "cds.h"

uint16_t CDS_Read(void)
{
	ADC_ChannelConfTypeDef sConfig = {0};

// ADC1을 PA5 (CDS)용 채널 5로 설정
sConfig.Channel = ADC_CHANNEL_5;
sConfig.Rank = 1;  // 첫 번째 변환 우선순위
sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    // ADC 변환 시작
    HAL_ADC_Start(&hadc1);

    // 변환 완료 대기
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        // 변환 값 읽기
        uint16_t cdsAdc = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);  // ADC 변환 정지
        return cdsAdc;
    }
    return 0;
}

void Set_Pin_ANALOG_CDS (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;


	//GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

CDS_Typedef CDS_readData()
{
	CDS_Typedef cds;
	Set_Pin_ANALOG_CDS(CDS_PORT, CDS_PIN);
	cds.cds_value = CDS_Read();
	return cds;
}
