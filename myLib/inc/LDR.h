#ifndef __LDR_H
#define __LDR_H

#include "stm32f1xx_hal.h"
uint16_t LDR_Read_ADC(ADC_HandleTypeDef *hadc);
uint16_t LDR_Read_ADC_Average(ADC_HandleTypeDef *hadc, uint8_t samples);
#endif

