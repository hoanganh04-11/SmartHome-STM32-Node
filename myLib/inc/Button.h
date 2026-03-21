#ifndef __BUTTON_H
#define __BUTTON_H

#include "main.h"


typedef struct
{
	uint8_t btn_current;
	uint8_t btn_last;
	uint8_t btn_filter;
	uint8_t is_debouncing;
	uint32_t time_debounce;
	uint32_t time_start_press;
	uint8_t is_press_timeout;
	GPIO_TypeDef *GPIOx;
	uint16_t GPIO_PIN;
}Button_Typedef;



void button_handle(Button_Typedef *ButtonX);
void button_init(Button_Typedef *ButtonX,GPIO_TypeDef *GPIOx, uint16_t GPIO_PIN);

#endif
