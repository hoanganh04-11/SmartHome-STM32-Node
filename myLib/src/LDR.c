#include "LDR.h"

uint16_t LDR_Read_ADC(ADC_HandleTypeDef *hadc)
{
    HAL_ADC_Start(hadc);

    if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK)
    {
        uint16_t value = HAL_ADC_GetValue(hadc);
        HAL_ADC_Stop(hadc);
        return value;
    }

    HAL_ADC_Stop(hadc);
    return 0;
}

uint16_t LDR_Read_ADC_Average(ADC_HandleTypeDef *hadc, uint8_t samples)
{
    uint32_t sum = 0;

    if (samples == 0) return 0;

    for (uint8_t i = 0; i < samples; i++)
    {
        sum += LDR_Read_ADC(hadc);
    }

    return (uint16_t)(sum / samples);
}