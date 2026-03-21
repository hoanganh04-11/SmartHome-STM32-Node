#include "Button.h"




__weak void btn_pressing_callback(Button_Typedef *ButtonX)
{

}

__weak void btn_press_short_callback(Button_Typedef *ButtonX)
{
	
}

__weak void btn_release_callback(Button_Typedef *ButtonX)
{
	
}

__weak void btn_press_timeout_callback(Button_Typedef *ButtonX)
{
	
}

void button_handle(Button_Typedef *ButtonX)
{
	// -----------loc nhieu 
	uint8_t sta = HAL_GPIO_ReadPin(ButtonX->GPIOx, ButtonX->GPIO_PIN);
	
	if(sta != ButtonX->btn_filter)
	{
		ButtonX->btn_filter = sta;
		ButtonX->is_debouncing = 1;
		ButtonX->time_debounce = HAL_GetTick();
	}
	// --------------tin hieu da xac lap
	if(ButtonX->is_debouncing && (HAL_GetTick() - ButtonX->time_debounce >= 15))
	{
		ButtonX->btn_current = ButtonX->btn_filter;
		ButtonX->is_debouncing = 0;
	}
	
	//-------------xu ly nhan nha ---------
	if(ButtonX->btn_current != ButtonX->btn_last)
	{
		if(ButtonX->btn_current == 0) //nhan xuong
		{
			ButtonX->is_press_timeout = 1;
			btn_pressing_callback(ButtonX);
			ButtonX->time_start_press = HAL_GetTick();
		}
		else //nha nut
		{
			if(HAL_GetTick() - ButtonX->time_start_press <= 1000)
			{
				btn_press_short_callback(ButtonX);
			}
			btn_release_callback(ButtonX); 
			ButtonX->is_press_timeout = 0;
		}
		ButtonX->btn_last = ButtonX->btn_current;
	}
	//----------- xu ly nhan giu --------
	if(ButtonX->is_press_timeout && (HAL_GetTick() - ButtonX->time_start_press >= 3000))
	{
		btn_press_timeout_callback(ButtonX);
		ButtonX->is_press_timeout = 0;
	}
}


void button_init(Button_Typedef *ButtonX,GPIO_TypeDef *GPIOx, uint16_t GPIO_PIN)
{
	ButtonX->GPIOx = GPIOx;
	ButtonX->GPIO_PIN = GPIO_PIN;
}
