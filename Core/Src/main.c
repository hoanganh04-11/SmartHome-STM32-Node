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
#include "Button.h"
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

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define DHT_READ_PERIOD_MS       2000U
#define ANALOG_READ_PERIOD_MS     200U
#define LCD_UPDATE_PERIOD_MS      500U
#define BUTTON_SCAN_PERIOD_MS      20U
#define LORA_SEND_PERIOD_MS     10000U
#define DHT_SENSOR_COUNT            2U

#define MQ2_SAMPLE_COUNT 10
#define LDR_SAMPLE_COUNT 10

#define MQ2_THRESHOLD_ON 2500U
#define MQ2_THRESHOLD_OFF 2300U

#define LDR_THRESHOLD_ON 2500U
#define LDR_THRESHOLD_OFF 2300U
#define NODE_ID 1U
#define FRAME_START_BYTE 0xAAU
#define FRAME_END_BYTE   0x55U

typedef enum
{
    SYSTEM_STATE_BOOT,
    SYSTEM_STATE_READ_DHT,
    SYSTEM_STATE_READ_ANALOG,
    SYSTEM_STATE_SCAN_BUTTONS,
    SYSTEM_STATE_CONTROL_OUTPUTS,
    SYSTEM_STATE_UPDATE_SCREEN,
    SYSTEM_STATE_SEND_PACKET,
    SYSTEM_STATE_IDLE
} SystemState_t;

typedef struct
{
    SystemState_t state;
    uint32_t tick_dht;
    uint32_t tick_analog;
    uint32_t tick_lcd;
    uint32_t tick_button;
    uint32_t tick_lora;
    uint8_t dht_valid[DHT_SENSOR_COUNT];
    uint8_t temperature[DHT_SENSOR_COUNT];
    uint8_t humidity[DHT_SENSOR_COUNT];
    uint8_t temp_avg;
    uint8_t hum_avg;
    uint8_t dht_avg_valid;
    uint16_t mq2_adc;
    uint16_t ldr_adc;
    uint8_t relay1_on;
    uint8_t relay2_on;
    uint8_t buzzer_on;
    uint8_t auto_mode;
    uint8_t lcd_page;
    uint8_t dht_display_index;
    uint8_t send_now;
    uint8_t gas_alarm_active;
} SystemContext_t;

#pragma pack(push, 1)
typedef struct
{
    uint8_t nodeId;
    float temperature;
    float humidity;
    uint16_t gasValue;
    uint16_t lightValue;
    uint8_t relayStatus;
    uint8_t buzzerStatus;
} SensorData_t;

typedef struct
{
    uint8_t startByte;
    SensorData_t payload;
    uint8_t checksum;
    uint8_t endByte;
} SensorFrame_t;
#pragma pack(pop)

DHT11_InitTypedef dht11_nodes[DHT_SENSOR_COUNT];
I2C_LCD_HandleTypedef lcd1;

Button_Typedef button_mode;
Button_Typedef button_relay1;
Button_Typedef button_relay2;
Button_Typedef button_view;

Relay_HandleTypeDef relay1 = {Relay1_GPIO_Port, Relay1_Pin, GPIO_PIN_RESET, RELAY_OFF};
Relay_HandleTypeDef relay2 = {Relay2_GPIO_Port, Relay2_Pin, GPIO_PIN_RESET, RELAY_OFF};

SystemContext_t system_ctx;

char lcd_line1[17];
char lcd_line2[17];

volatile uint8_t event_toggle_mode = 0U;
volatile uint8_t event_toggle_relay1 = 0U;
volatile uint8_t event_toggle_relay2 = 0U;
volatile uint8_t event_toggle_lcd_page = 0U;
volatile uint8_t event_send_now = 0U;

uint8_t lora_rx_byte;
uint8_t lora_rx_buffer[sizeof(SensorFrame_t)];
uint8_t lora_rx_idx = 0;

static uint8_t IsTimeElapsed(uint32_t *last_tick, uint32_t period_ms)
{
    uint32_t now = HAL_GetTick();
    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1U;
    }
    return 0U;
}

static void LCD_PrintLine(uint8_t row, const char *text)
{
    char buf[17];
    size_t len = 0U;
    memset(buf, ' ', 16);
    buf[16] = '\0';
    if (text != NULL)
    {
        while ((len < 16U) && (text[len] != '\0'))
        {
            len++;
        }
        memcpy(buf, text, len);
    }
    lcd_gotoxy(&lcd1, 0, row);
    lcd_puts(&lcd1, buf);
}

static void Buzzer_Set(uint8_t on)
{
    HAL_GPIO_WritePin(CoiChip_GPIO_Port, CoiChip_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t CalculateXorChecksum(const uint8_t *data, uint16_t len)
{
    uint8_t checksum = 0U;
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        checksum ^= data[i];
    }

    return checksum;
}

static void InitButtonRuntime(Button_Typedef *button)
{
    uint8_t current = HAL_GPIO_ReadPin(button->GPIOx, button->GPIO_PIN);
    button->btn_current = current;
    button->btn_last = current;
    button->btn_filter = current;
}
static void ReadDhtGroup(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_dht, DHT_READ_PERIOD_MS))
    {
        uint16_t temp_sum = 0U;
        uint16_t hum_sum = 0U;
        uint8_t valid_count = 0U;
        uint8_t i;
        for (i = 0U; i < DHT_SENSOR_COUNT; i++)
        {
            ctx->dht_valid[i] = readDHT11(&dht11_nodes[i]);
            if (ctx->dht_valid[i] != 0U)
            {
                ctx->temperature[i] = dht11_nodes[i].temperature;
                ctx->humidity[i] = dht11_nodes[i].humidity;
                temp_sum += ctx->temperature[i];
                hum_sum += ctx->humidity[i];
                valid_count++;
            }
        }

        if (valid_count > 0U)
        {
            ctx->temp_avg = (uint8_t)(temp_sum / valid_count);
            ctx->hum_avg = (uint8_t)(hum_sum / valid_count);
            ctx->dht_avg_valid = 1U;
        }
        else
        {
            ctx->dht_avg_valid = 0U;
        }
    }
}

static void ReadAnalogSensors(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_analog, ANALOG_READ_PERIOD_MS))
    {
        ctx->mq2_adc = MQ2_Read_ADC_Average(&hadc1, MQ2_SAMPLE_COUNT);
        ctx->ldr_adc = LDR_Read_ADC_Average(&hadc2, LDR_SAMPLE_COUNT);
    }
}

static void ScanButtons(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_button, BUTTON_SCAN_PERIOD_MS))
    {
        button_handle(&button_mode);
        button_handle(&button_relay1);
        button_handle(&button_relay2);
        button_handle(&button_view);
    }

    if (event_toggle_mode != 0U)
    {
        ctx->auto_mode ^= 1U;
        event_toggle_mode = 0U;
    }

    if ((ctx->auto_mode == 0U) && (event_toggle_relay1 != 0U))
    {
        ctx->relay1_on ^= 1U;
    }
    event_toggle_relay1 = 0U;

    if ((ctx->auto_mode == 0U) && (event_toggle_relay2 != 0U))
    {
        ctx->relay2_on ^= 1U;
    }
    event_toggle_relay2 = 0U;

    if (event_toggle_lcd_page != 0U)
    {
        ctx->lcd_page ^= 1U;
        event_toggle_lcd_page = 0U;
    }

    if (event_send_now != 0U)
    {
        ctx->send_now = 1U;
        event_send_now = 0U;
    }
}

static void ControlOutputs(SystemContext_t *ctx)
{
    if (ctx->mq2_adc >= MQ2_THRESHOLD_ON)
    {
        ctx->gas_alarm_active = 1U;
    }
    else if (ctx->mq2_adc <= MQ2_THRESHOLD_OFF)
    {
        ctx->gas_alarm_active = 0U;
    }

    if (ctx->auto_mode != 0U)
    {
        ctx->relay1_on = (ctx->gas_alarm_active != 0U) ? 1U : 0U;

        if (ctx->ldr_adc >= LDR_THRESHOLD_ON)
        {
            ctx->relay2_on = 1U;
        }
        else if (ctx->ldr_adc <= LDR_THRESHOLD_OFF)
        {
            ctx->relay2_on = 0U;
        }
    }

    ctx->buzzer_on = ctx->gas_alarm_active;
    Relay_SetState(&relay1, ctx->relay1_on ? RELAY_ON : RELAY_OFF);
    Relay_SetState(&relay2, ctx->relay2_on ? RELAY_ON : RELAY_OFF);
    Buzzer_Set(ctx->buzzer_on);
}

static void UpdateScreen(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_lcd, LCD_UPDATE_PERIOD_MS))
    {
        if (ctx->lcd_page == 0U)
        {
            if (ctx->dht_avg_valid != 0U)
            {
							snprintf(lcd_line1, sizeof(lcd_line1), "T:%2uC H:%2u%%", ctx->temp_avg, ctx->hum_avg);
            }
            else
            {
                snprintf(lcd_line1, sizeof(lcd_line1), "DHT Invalid");
            }
            snprintf(lcd_line2, sizeof(lcd_line2), "G:%4u L:%4u", ctx->mq2_adc, ctx->ldr_adc);
        }
        else
        {
            uint8_t idx = ctx->dht_display_index;
            if (ctx->dht_valid[idx] != 0U)
            {
                snprintf(lcd_line1, sizeof(lcd_line1), "D%u T:%2u H:%2u",
                         (unsigned int)(idx + 1U),
                         (unsigned int)ctx->temperature[idx],
                         (unsigned int)ctx->humidity[idx]);
            }
            else
            {
                snprintf(lcd_line1, sizeof(lcd_line1), "D%u Read Fail", (unsigned int)(idx + 1U));
            }
            snprintf(lcd_line2, sizeof(lcd_line2), "%s R1:%u R2:%u",
                     (ctx->auto_mode != 0U) ? "AUTO" : "MAN ",
                     (unsigned int)ctx->relay1_on,
                     (unsigned int)ctx->relay2_on);
            ctx->dht_display_index++;
            if (ctx->dht_display_index >= DHT_SENSOR_COUNT)
            {
                ctx->dht_display_index = 0U;
            }
        }

        LCD_PrintLine(0, lcd_line1);
        LCD_PrintLine(1, lcd_line2);
    }
}

static void SendTelemetry(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_lora, LORA_SEND_PERIOD_MS) || (ctx->send_now != 0U))
    {
        SensorFrame_t frame;

        frame.startByte = FRAME_START_BYTE;
        frame.payload.nodeId = NODE_ID;
        frame.payload.temperature = (float)ctx->temp_avg;
        frame.payload.humidity = (float)ctx->hum_avg;
        frame.payload.gasValue = ctx->mq2_adc;
        frame.payload.lightValue = ctx->ldr_adc;
        frame.payload.relayStatus = (uint8_t)((ctx->relay1_on ? 0x01U : 0x00U) |
                                              (ctx->relay2_on ? 0x02U : 0x00U));
        frame.payload.buzzerStatus = ctx->buzzer_on;
        frame.checksum = CalculateXorChecksum((const uint8_t*)&frame.payload, sizeof(frame.payload));
        frame.endByte = FRAME_END_BYTE;

        HAL_UART_Transmit(&huart1, (uint8_t*)&frame, sizeof(frame), 100);
        ctx->send_now = 0U;
    }
}

static void RunSystemStateMachine(SystemContext_t *ctx)
{
    switch (ctx->state)
    {
        case SYSTEM_STATE_BOOT:
            ctx->state = SYSTEM_STATE_READ_DHT;
            break;

        case SYSTEM_STATE_READ_DHT:
            ReadDhtGroup(ctx);
            ctx->state = SYSTEM_STATE_READ_ANALOG;
            break;

        case SYSTEM_STATE_READ_ANALOG:
            ReadAnalogSensors(ctx);
            ctx->state = SYSTEM_STATE_SCAN_BUTTONS;
            break;

        case SYSTEM_STATE_SCAN_BUTTONS:
            ScanButtons(ctx);
            ctx->state = SYSTEM_STATE_CONTROL_OUTPUTS;
            break;

        case SYSTEM_STATE_CONTROL_OUTPUTS:
            ControlOutputs(ctx);
            ctx->state = SYSTEM_STATE_UPDATE_SCREEN;
            break;

        case SYSTEM_STATE_UPDATE_SCREEN:
            UpdateScreen(ctx);
            ctx->state = SYSTEM_STATE_SEND_PACKET;
            break;

        case SYSTEM_STATE_SEND_PACKET:
            SendTelemetry(ctx);
            ctx->state = SYSTEM_STATE_IDLE;
            break;

        case SYSTEM_STATE_IDLE:
        default:
            ctx->state = SYSTEM_STATE_READ_DHT;
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	//------------------DHT11----------------------
	DHT11_Init(&dht11_nodes[0], &htim1, GPIOA, GPIO_PIN_11);
	DHT11_Init(&dht11_nodes[1], &htim1, GPIOA, GPIO_PIN_6);

	//------------------LCD------------------------
	HAL_Delay(100);
	lcd1.hi2c = &hi2c1;
	lcd1.address = LCD_ADDRESS;

	lcd_init(&lcd1);
	lcd_clear(&lcd1);
	//------------------RELAY-----------------------
	Relay_Init(&relay1);
	Relay_Init(&relay2);
	Buzzer_Set(0);

	memset(&system_ctx, 0, sizeof(system_ctx));
	system_ctx.state = SYSTEM_STATE_BOOT;
	system_ctx.auto_mode = 1U;

	button_init(&button_mode, GPIOA, GPIO_PIN_12);  // A12
	button_init(&button_relay1, GPIOA, GPIO_PIN_15); // A15
	button_init(&button_relay2, GPIOB, GPIO_PIN_3);  // B3
	button_init(&button_view, GPIOB, GPIO_PIN_4);    // B4

	InitButtonRuntime(&button_mode);
	InitButtonRuntime(&button_relay1);
	InitButtonRuntime(&button_relay2);
	InitButtonRuntime(&button_view);

	LCD_PrintLine(0, "System Booting");
	LCD_PrintLine(1, "STM32 IoT Node");	

	// Bắt đầu nhận dữ liệu từ Lora (qua ngắt UART)
	HAL_UART_Receive_IT(&huart1, &lora_rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		RunSystemStateMachine(&system_ctx);
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, Relay2_Pin|Relay1_Pin|GPIO_PIN_6|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CoiChip_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Relay2_Pin Relay1_Pin PA6 PA11 */
  GPIO_InitStruct.Pin = Relay2_Pin|Relay1_Pin|GPIO_PIN_6|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Button1_Pin Button2_Pin */
  GPIO_InitStruct.Pin = Button1_Pin|Button2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : Button4_Pin */
  GPIO_InitStruct.Pin = Button4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Button4_GPIO_Port, &GPIO_InitStruct);

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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (lora_rx_byte == FRAME_START_BYTE)
        {
            lora_rx_idx = 0U;
            lora_rx_buffer[lora_rx_idx++] = lora_rx_byte;
        }
        else if (lora_rx_idx > 0U)
        {
            if (lora_rx_idx < sizeof(SensorFrame_t))
            {
                lora_rx_buffer[lora_rx_idx++] = lora_rx_byte;
            }

            if (lora_rx_idx == sizeof(SensorFrame_t))
            {
                SensorFrame_t rx_frame;
                uint8_t expected_checksum;

                memcpy(&rx_frame, lora_rx_buffer, sizeof(SensorFrame_t));

                if ((rx_frame.startByte == FRAME_START_BYTE) && (rx_frame.endByte == FRAME_END_BYTE))
                {
                    expected_checksum = CalculateXorChecksum((const uint8_t *)&rx_frame.payload, sizeof(rx_frame.payload));
                    if ((rx_frame.checksum == expected_checksum) && (system_ctx.auto_mode == 0U))
                    {
                        system_ctx.relay1_on = ((rx_frame.payload.relayStatus & 0x01U) != 0U) ? 1U : 0U;
                        system_ctx.relay2_on = ((rx_frame.payload.relayStatus & 0x02U) != 0U) ? 1U : 0U;
                    }
                }

                lora_rx_idx = 0U;
            }
        }
        HAL_UART_Receive_IT(&huart1, &lora_rx_byte, 1);
    }
}

void btn_press_short_callback(Button_Typedef *ButtonX)
{
  if (ButtonX == &button_mode)
  {
    event_toggle_mode = 1U;
  }
  else if (ButtonX == &button_relay1)
  {
    event_toggle_relay1 = 1U;
  }
  else if (ButtonX == &button_relay2)
  {
    event_toggle_relay2 = 1U;
  }
  else if (ButtonX == &button_view)
  {
    event_toggle_lcd_page = 1U;
  }
}

void btn_press_timeout_callback(Button_Typedef *ButtonX)
{
  if (ButtonX == &button_view)
  {
    event_send_now = 1U;
  }
}

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
