
#include "water.h"

uint16_t WATER_Read (void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // ADC1을 PA4 (Water)용 채널 4로 설정
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    // ADC 변환 시작
    HAL_ADC_Start(&hadc1);

    // 변환 완료 대기
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        // 변환 값 읽기
        uint16_t waterAdc = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);  // ADC 변환 정지
        return waterAdc;
    }
    return 0;
}

WATER_Typedef WATER_readData()
{
	WATER_Typedef water;
	water.water_value = WATER_Read();
	return water;
}

void Set_Pin_ANALOG_WATER (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;


	//GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}
