#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f1xx_hal.h"

typedef enum
{
    RELAY_OFF = 0,
    RELAY_ON  = 1
} Relay_State_t;

typedef struct
{
    GPIO_TypeDef *GPIOx;
    uint16_t pin;
    GPIO_PinState active_level;
    Relay_State_t state;
} Relay_HandleTypeDef;

void Relay_Init(Relay_HandleTypeDef *relay);
void Relay_SetState(Relay_HandleTypeDef *relay, Relay_State_t state);
void Relay_On(Relay_HandleTypeDef *relay);
void Relay_Off(Relay_HandleTypeDef *relay);

#endif

