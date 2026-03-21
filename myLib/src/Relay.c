#include "Relay.h"

static void Relay_Write(Relay_HandleTypeDef *relay, Relay_State_t state)
{
    GPIO_PinState pin_state;

    if (state == RELAY_ON)
        pin_state = relay->active_level;
    else
        pin_state = (relay->active_level == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;

    HAL_GPIO_WritePin(relay->GPIOx, relay->pin, pin_state);
    relay->state = state;
}

void Relay_Init(Relay_HandleTypeDef *relay)
{
    Relay_Write(relay, RELAY_OFF);
}

void Relay_SetState(Relay_HandleTypeDef *relay, Relay_State_t state)
{
    Relay_Write(relay, state);
}

void Relay_On(Relay_HandleTypeDef *relay)
{
    Relay_Write(relay, RELAY_ON);
}

void Relay_Off(Relay_HandleTypeDef *relay)
{
    Relay_Write(relay, RELAY_OFF);
}