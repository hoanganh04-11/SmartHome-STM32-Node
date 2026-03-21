#include "LCD_I2C.h"


void lcd_send_cmd(I2C_LCD_HandleTypedef *lcd, char cmd) //gui cmd
{
	char upper_nibble, lower_nibble;
	uint8_t data_t[4];
	
	upper_nibble = (cmd & 0xF0);
	lower_nibble = ((cmd << 4) & 0xF0);
	
	data_t[0] = upper_nibble | 0x0C; //en = 1, rs = 0: 0x0C = 0000 1100 (bit 0: RS = 0/1 (cmd/data), bit 1: RW = 0 (Write), bit 2: En = 1/0)
	data_t[1] = upper_nibble | 0x08; //en = 0, rs = 0 thiet lap
	data_t[2] = lower_nibble | 0x0C; //en = 1, rs = 0
	data_t[3] = lower_nibble | 0x08; //en = 0, rs = 0 thiet lap

	
	HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, data_t, 4, 100);
}


void lcd_send_data(I2C_LCD_HandleTypedef *lcd, char data) //gui data 
{
	char upper_nibble, lower_nibble;
	uint8_t data_t[4];
	
	upper_nibble = (data & 0xF0);
	lower_nibble = ((data << 4) & 0xF0);
	
	data_t[0] = upper_nibble | 0x0D; //en = 1, rs = 1: 0x0D = 0000 1101 
	data_t[1] = upper_nibble | 0x09; //en = 0, rs = 1
	data_t[2] = lower_nibble | 0x0D; //end = 1, rs = 1
	data_t[3] = lower_nibble | 0x09; //en = 0, rs = 1

	
	HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, data_t, 4, 100);
}

void lcd_init(I2C_LCD_HandleTypedef *lcd) //khoi tao lcd
{
	HAL_Delay(50);
	lcd_send_cmd(lcd, 0x30); // wake up command
	HAL_Delay(5); 
	lcd_send_cmd(lcd, 0x30); // wake up command
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x30); // wake up command
	HAL_Delay(10);
	lcd_send_cmd(lcd, 0x20); //set len 4 bit mode
	HAL_Delay(10);
	
	//LCd configuration
	lcd_send_cmd(lcd, 0x28); // 4 bit mode, 2 lines, 5x8 font
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x08); // display off, cursor off, blink off
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x01); // clear display
	HAL_Delay(2);
	lcd_send_cmd(lcd, 0x06); // Entry mode: cursor movers right
	HAL_Delay(1);
	lcd_send_cmd(lcd, 0x0C); // Display on, cursor off, blink off
}

void lcd_putchar(I2C_LCD_HandleTypedef *lcd, char ch) //in ky tu
{
	lcd_send_data(lcd, ch);
}


void lcd_puts(I2C_LCD_HandleTypedef *lcd, char *str) //in string
{
	while(*str)
	{
		lcd_send_data(lcd, *str++);
	}
}


void lcd_gotoxy(I2C_LCD_HandleTypedef *lcd, uint8_t col, uint8_t row) //di chuyen con tro toi vi tri cot x, hang y
{
	uint8_t address;
	
	switch(row)
	{
		case 0: //hang 1
			address = 0x80 + col;
			break;
		case 1: //hang 2
			address = 0xC0 + col;
			break;
		default:
			return;
	}
		lcd_send_cmd(lcd, address);
}


void lcd_clear(I2C_LCD_HandleTypedef *lcd) // clear man hinh
{
	lcd_send_cmd(lcd, 0x01);
	HAL_Delay(2);
}


