#include "DHT11.h"

void DHT11_Init(DHT11_InitTypedef *dht, TIM_HandleTypeDef *htim, GPIO_TypeDef *GPIOx, uint16_t pin)
{
	dht->htim = htim;
	dht->GPIOx = GPIOx;
	dht->pin = pin;
}
void set_dht11_gpio_mode(DHT11_InitTypedef *dht, uint8_t pMode)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	if(pMode == OUTPUT)
	{
		GPIO_InitStruct.Pin = dht->pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		
		HAL_GPIO_Init(dht->GPIOx, &GPIO_InitStruct);
	}
	
	else if(pMode == INPUT)
	{
		GPIO_InitStruct.Pin = dht->pin;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		
		HAL_GPIO_Init(dht->GPIOx, &GPIO_InitStruct);
	}
}
uint8_t readDHT11(DHT11_InitTypedef *dht)
{
	uint16_t mTime1 = 0, mTime2 = 0, mBit = 0;
	uint8_t humVal = 0, tempVal = 0, parityVal = 0, genParity = 0;
	uint8_t mData[40];
	
	//bat dau giao tiep
	set_dht11_gpio_mode(dht, OUTPUT); 
	HAL_GPIO_WritePin(dht->GPIOx, dht->pin, 0);
	HAL_Delay(18);	// cho 18 ms den Low state 
	__disable_irq();	//tat tat ca cac ngat chi de doc dht 
	
	HAL_TIM_Base_Start(dht->htim);	//bat dau dem timer
	set_dht11_gpio_mode(dht, INPUT);
	
	//kiem tra phan hoi dht (dht response)
	
	__HAL_TIM_SET_COUNTER(dht->htim, 0);	//reset bo dem ve 0
	while(HAL_GPIO_ReadPin(dht->GPIOx, dht->pin) == 1)	//cho muc high 1->0
	{
		if((uint16_t)__HAL_TIM_GET_COUNTER(dht->htim) > 500)	//doc gia tri counter hien tai neu > 500us thi return 9
		{
			__enable_irq();
			return 0;
		}
	}
	
	//Do do dai muc Low cua ACK (~80us)
	__HAL_TIM_SET_COUNTER(dht->htim, 0); //reset bo dem ve 0
	while(HAL_GPIO_ReadPin(dht->GPIOx, dht->pin) == 0) //do do dai khi data = 0
	{
		if((uint16_t)__HAL_TIM_GET_COUNTER(dht->htim) > 500)	//doc gia tri counter hien tai neu > 500us thi return 9
		{
			__enable_irq();
			return 0;
		}
	}	
	mTime1 = (uint16_t)__HAL_TIM_GET_COUNTER(dht->htim); //thoi gian o muc low duoc gan cho tim1 (low -> high)
	
	//Do do dai muc High cua ACK (~80us)
	__HAL_TIM_SET_COUNTER(dht->htim, 0);	//reset bo dem ve 0
	while(HAL_GPIO_ReadPin(dht->GPIOx, dht->pin) == 1)	//do do dai khi data = 0
	{
		if((uint16_t)__HAL_TIM_GET_COUNTER(dht->htim) > 500)	//doc gia tri counter hien tai neu > 500us thi return 9
		{
			__enable_irq();
			return 0;
		}
	}
	mTime2 = (uint16_t)__HAL_TIM_GET_COUNTER(dht->htim);	//thoi gian o muc high duoc gan cho tim1 (high -> low)
	
	//check kiem tra tra loi sai (if answer is wrong return)
	if((mTime1 < 75 || mTime1 > 85) || (mTime2 < 75 || mTime2 > 85))
	{
		__enable_irq();
		return 0;
	}
	
	//neu dung thi tiep tuc doc 40 bit
	for(int j = 0; j < 40; j++)
	{
		__HAL_TIM_SET_COUNTER(dht->htim, 0);	//reset bo dem ve 0
		while(HAL_GPIO_ReadPin(dht->GPIOx, dht->pin) == 0)	//doi het low 50us
		{
			if((uint16_t)__HAL_TIM_GET_COUNTER(dht->htim) > 500)
			{
				__enable_irq();
				return 0;
			}
		}
		
		__HAL_TIM_SET_COUNTER(dht->htim, 0);	//reset bo dem ve 0
		while(HAL_GPIO_ReadPin(dht->GPIOx, dht->pin) == 1)	//do thoi gian high
		{
			if((uint16_t)__HAL_TIM_GET_COUNTER(dht->htim) > 500)
			{
				__enable_irq();
				return 0;
			}
		}
		
		
		mTime1 = (uint16_t)__HAL_TIM_GET_COUNTER(dht->htim); //mTime1 la thoi gian muc high (dung de check bit 0 va 1)
		
		//kiem tra thoi gian o muc cao (check pass time in hight state)
		//neu thoi gian qua 25uS roi xuong muc thap (if pass time 25uS set as Low)
		if(mTime1 > 20 && mTime1 < 30)
		{
			mBit = 0;
		}
		else if(mTime1 > 60 && mTime1 < 80)	//neu thoi gian qua 70us roi len muc cao (if pass time 70 uS set as High)
		{
			mBit = 1;
		}
		
		//set i th data in data buffer
		mData[j] = mBit;
		
	}
	
	
	HAL_TIM_Base_Stop(dht->htim); //dung timer
	__enable_irq(); //enable all interrupts
	
	//nhan gia tri do am tu data buffer
	for(int i = 0; i < 8; i++)
	{
		humVal = (humVal << 1) | (mData[i] & 1);
	}
	
	uint8_t hum_dec = 0, temp_dec = 0;
	
	//phan thap phan cua do am
	for(int i = 8; i < 16; i++)
	{
		hum_dec = (hum_dec << 1) | (mData[i] & 1);
	}
	
	//nhan gia tri nhiet do tu data buffer
	for(int i = 16; i < 24; i++)
	{
		tempVal = (tempVal << 1) | (mData[i] & 1);
	}
	
	//phan thap phan cua nhiet do
	for(int i = 24; i < 32; i++)
	{
		temp_dec = (temp_dec << 1) | (mData[i] & 1);
	}
	
	//get parity value form data buffer
	for(int i = 32; i < 40; i++)
	{
		parityVal = (parityVal << 1) | (mData[i] & 1);
	}
	
	genParity = (uint8_t)(humVal + hum_dec + tempVal +temp_dec);
	
	if(genParity != parityVal)
	{
		return 0;
	}
	
	dht->temperature = tempVal;
	dht->humidity = humVal;
	
	
	return 1;
}

