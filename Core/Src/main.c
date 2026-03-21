/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LCD_I2C.h"
#include "DHT11.h"
#include "Relay.h"
#include "stdio.h"
#include "string.h"
#include "Mq2.h"
#include "LDR.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define LCD_COLS 16
#define DHT_READ_PERIOD_MS 2000U
#define ANALOG_READ_PERIOD_MS 200U
#define LCD_UPDATE_PERIOD_MS 500U
#define LORA_SEND_PERIOD_MS 3000U

#define MQ2_SAMPLE_COUNT 10
#define LDR_SAMPLE_COUNT 10

#define MQ2_THRESHOLD_ON 2500U
#define MQ2_THRESHOLD_OFF 2300U

#define LDR_THRESHOLD_ON 2500U
#define LDR_THRESHOLD_OFF 2300U

typedef enum
{
	APP_STATE_BOOT_INIT,
	APP_STATE_SAMPLE_DHT11,
	APP_STATE_SAMPLE_ANALOG,
	APP_STATE_PROCESS_CONTROL,
	APP_STATE_UPDATE_LCD,
	APP_STATE_SEND_LORA,
	APP_STATE_IDLE,
}AppState_t;

typedef struct
{
    AppState_t state;

    uint32_t tick_dht;
    uint32_t tick_analog;
    uint32_t tick_lcd;
    uint32_t tick_lora;

    uint8_t  dht_valid;
    uint8_t  temperature;
    uint8_t  humidity;

    uint16_t mq2_adc;
    uint16_t ldr_adc;

    uint8_t relay1_on;
    uint8_t relay2_on;
    uint8_t buzzer_on;
} AppContext_t;

DHT11_InitTypedef dht11;
I2C_LCD_HandleTypedef lcd1;

Relay_HandleTypeDef relay1 = {Relay1_GPIO_Port, Relay1_Pin, GPIO_PIN_RESET, RELAY_OFF};
Relay_HandleTypeDef relay2 = {Relay2_GPIO_Port, Relay2_Pin, GPIO_PIN_RESET, RELAY_OFF};

AppContext_t app;

char lcd_line1[17];
char lcd_line2[17];

static uint8_t App_IsTimeElapsed(uint32_t *last_tick, uint32_t period_ms)
{
    uint32_t now = HAL_GetTick();

    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1;
    }
    return 0;
}

static void LCD_PrintLine(uint8_t row, const char *text)
{
    char buf[17];
    memset(buf, ' ', 16);
    buf[16] = '\0';

    strncpy(buf, text, 16);

    lcd_gotoxy(&lcd1, 0, row);
    lcd_puts(&lcd1, buf);
}

static void Buzzer_Set(uint8_t on)
{
    HAL_GPIO_WritePin(CoiChip_GPIO_Port, CoiChip_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void App_SampleDHT11(AppContext_t *ctx)
{
    if (App_IsTimeElapsed(&ctx->tick_dht, DHT_READ_PERIOD_MS))
    {
        ctx->dht_valid = readDHT11(&dht11);

        if (ctx->dht_valid)
        {
            ctx->temperature = dht11.temperature;
            ctx->humidity    = dht11.humidity;
        }
    }
}

static void App_SampleAnalog(AppContext_t *ctx)
{
    if (App_IsTimeElapsed(&ctx->tick_analog, ANALOG_READ_PERIOD_MS))
    {
        ctx->mq2_adc = MQ2_Read_ADC_Average(&hadc1, MQ2_SAMPLE_COUNT);
        ctx->ldr_adc = LDR_Read_ADC_Average(&hadc2, LDR_SAMPLE_COUNT);
    }
}

static void App_ProcessControl(AppContext_t *ctx)
{
    /* MQ2 -> Relay1 + Coi */
    if (ctx->mq2_adc >= MQ2_THRESHOLD_ON)
    {
        ctx->relay1_on = 1;
        ctx->buzzer_on = 1;
    }
    else if (ctx->mq2_adc <= MQ2_THRESHOLD_OFF)
    {
        ctx->relay1_on = 0;
        ctx->buzzer_on = 0;
    }

    /* LDR -> Relay2
       Lưu ý: nếu mạch chia áp bị đảo logic thì đổi dấu so sánh lại */
    if (ctx->ldr_adc >= LDR_THRESHOLD_ON)
    {
        ctx->relay2_on = 1;
    }
    else if (ctx->ldr_adc <= LDR_THRESHOLD_OFF)
    {
        ctx->relay2_on = 0;
    }

    Relay_SetState(&relay1, ctx->relay1_on ? RELAY_ON : RELAY_OFF);
    Relay_SetState(&relay2, ctx->relay2_on ? RELAY_ON : RELAY_OFF);
    Buzzer_Set(ctx->buzzer_on);
}

static void App_UpdateLCD(AppContext_t *ctx)
{
    if (App_IsTimeElapsed(&ctx->tick_lcd, LCD_UPDATE_PERIOD_MS))
    {
        if (ctx->dht_valid)
        {
            snprintf(lcd_line1, sizeof(lcd_line1), "T:%2uC H:%2u%%", ctx->temperature, ctx->humidity);
        }
        else
        {
            snprintf(lcd_line1, sizeof(lcd_line1), "DHT11 Read Fail");
        }

        snprintf(lcd_line2, sizeof(lcd_line2), "G:%4u L:%4u", ctx->mq2_adc, ctx->ldr_adc);

        LCD_PrintLine(0, lcd_line1);
        LCD_PrintLine(1, lcd_line2);
    }
}

static void App_SendLoRa(AppContext_t *ctx)
{
    if (App_IsTimeElapsed(&ctx->tick_lora, LORA_SEND_PERIOD_MS))
    {
        /* Chưa có file UART/AS32 nên tôi để state này sẵn cho bạn.
           Sau khi cấu hình USART, bạn có thể gửi chuỗi kiểu:
           TEMP=xx,HUM=yy,MQ2=zzzz,LDR=zzzz,R1=x,R2=x,BZ=x\r\n
           
           Ví dụ:
           char tx_buf[80];
           int len = snprintf(tx_buf, sizeof(tx_buf),
                              "TEMP=%u,HUM=%u,MQ2=%u,LDR=%u,R1=%u,R2=%u,BZ=%u\r\n",
                              ctx->temperature, ctx->humidity, ctx->mq2_adc, ctx->ldr_adc,
                              ctx->relay1_on, ctx->relay2_on, ctx->buzzer_on);

           HAL_UART_Transmit_IT(&huart1, (uint8_t*)tx_buf, len);
        */
    }
}

static void App_RunStateMachine(AppContext_t *ctx)
{
    switch (ctx->state)
    {
        case APP_STATE_BOOT_INIT:
            ctx->state = APP_STATE_SAMPLE_DHT11;
            break;

        case APP_STATE_SAMPLE_DHT11:
            App_SampleDHT11(ctx);
            ctx->state = APP_STATE_SAMPLE_ANALOG;
            break;

        case APP_STATE_SAMPLE_ANALOG:
            App_SampleAnalog(ctx);
            ctx->state = APP_STATE_PROCESS_CONTROL;
            break;

        case APP_STATE_PROCESS_CONTROL:
            App_ProcessControl(ctx);
            ctx->state = APP_STATE_UPDATE_LCD;
            break;

        case APP_STATE_UPDATE_LCD:
            App_UpdateLCD(ctx);
            ctx->state = APP_STATE_SEND_LORA;
            break;

        case APP_STATE_SEND_LORA:
            App_SendLoRa(ctx);
            ctx->state = APP_STATE_IDLE;
            break;

        case APP_STATE_IDLE:
        default:
            ctx->state = APP_STATE_SAMPLE_DHT11;
            break;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
	//------------------DHT11----------------------
	DHT11_Init(&dht11, &htim1, GPIOA, GPIO_PIN_11);

	//------------------LCD------------------------
	lcd1.hi2c = &hi2c1;
	lcd1.address = LCD_ADDRESS;

	lcd_init(&lcd1);
	lcd_clear(&lcd1);
	//------------------RELAY-----------------------
	Relay_Init(&relay1);
	Relay_Init(&relay2);
	Buzzer_Set(0);

	memset(&app, 0, sizeof(app));
	app.state = APP_STATE_BOOT_INIT;

	LCD_PrintLine(0, "System Booting");
	LCD_PrintLine(1, "STM32 IoT Node");	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		App_RunStateMachine(&app);
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 63;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, Relay2_Pin|Relay1_Pin|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CoiChip_GPIO_Port, CoiChip_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Relay2_Pin Relay1_Pin PA11 */
  GPIO_InitStruct.Pin = Relay2_Pin|Relay1_Pin|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Button1_Pin Button2_Pin */
  GPIO_InitStruct.Pin = Button1_Pin|Button2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Button3_Pin Button4_Pin */
  GPIO_InitStruct.Pin = Button3_Pin|Button4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : CoiChip_Pin */
  GPIO_InitStruct.Pin = CoiChip_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CoiChip_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
