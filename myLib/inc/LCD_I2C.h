#ifndef __LCD_I2C_H
#define __LCD_I2C_H
#include "stm32f1xx_hal.h"

#define LCD_ADDRESS 0x27<<1

typedef struct
{
	I2C_HandleTypeDef *hi2c;
	uint8_t address;
}I2C_LCD_HandleTypedef;

void lcd_init(I2C_LCD_HandleTypedef *lcd); //khoi tao lcd
void lcd_send_cmd(I2C_LCD_HandleTypedef *lcd, char cmd); //gui cmd
void lcd_send_data(I2C_LCD_HandleTypedef *lcd, char data); //gui data 
void lcd_putchar(I2C_LCD_HandleTypedef *lcd, char ch); //in ky tu
void lcd_puts(I2C_LCD_HandleTypedef *lcd, char *str); //in string
void lcd_gotoxy(I2C_LCD_HandleTypedef *lcd, uint8_t col, uint8_t row); //di chuyen con tro toi vi tri cot x, hang y
void lcd_clear(I2C_LCD_HandleTypedef *lcd); // clear man hinh
#endif

