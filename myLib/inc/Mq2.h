#ifndef __MQ2_H
#define __MQ2_H

#include "stm32f1xx_hal.h"


uint16_t MQ2_Read_ADC(ADC_HandleTypeDef *hadc);
uint16_t MQ2_Read_ADC_Average(ADC_HandleTypeDef *hadc, uint8_t samples);

#endif
