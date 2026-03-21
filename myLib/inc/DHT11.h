#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"

#define OUTPUT 1
#define INPUT  0

typedef struct
{
    GPIO_TypeDef *GPIOx;
    uint16_t pin;
    TIM_HandleTypeDef *htim;
    uint8_t temperature;
    uint8_t humidity;
} DHT11_InitTypedef;

void DHT11_Init(DHT11_InitTypedef *dht, TIM_HandleTypeDef *htim, GPIO_TypeDef *GPIO, uint16_t pin);
void set_dht11_gpio_mode(DHT11_InitTypedef *dht, uint8_t pMode);
uint8_t readDHT11(DHT11_InitTypedef *dht);

#endif