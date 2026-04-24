/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 LoRa Node — Entry point
  *
  * Luồng hoạt động:
  *   Gateway (ESP32) gửi "senddata\n" mỗi 5 giây
  *     -> Node đọc cảm biến, đóng gói SensorData_t, gửi frame LoRa lên
  *   Gateway gửi binary SensorFrame_t khi người dùng thay đổi trên Blynk
  *     -> Node cập nhật ngưỡng, chế độ, relay
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "node_config.h"
#include "lora_node.h"
#include "system_control.h"
#include "node_buttons.h"
#include "LCD_I2C.h"
#include "DHT11.h"
#include "Relay.h"
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
/* Hardware instances — extern trong system_control.c */
DHT11_InitTypedef     dht11_nodes[DHT_SENSOR_COUNT];
I2C_LCD_HandleTypedef lcd1;
Relay_HandleTypeDef   relay1 = {Relay1_GPIO_Port, Relay1_Pin, GPIO_PIN_RESET, RELAY_OFF};
Relay_HandleTypeDef   relay2 = {Relay2_GPIO_Port, Relay2_Pin, GPIO_PIN_RESET, RELAY_OFF};

/* System context */
SystemContext_t system_ctx;
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

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */

  /* ---- DHT11 (2 sensors: PA6, PA11) ---- */
  DHT11_Init(&dht11_nodes[0], &htim1, GPIOA, GPIO_PIN_6);
  DHT11_Init(&dht11_nodes[1], &htim1, GPIOA, GPIO_PIN_11);

  /* ---- LCD I2C ---- */
  HAL_Delay(100);
  lcd1.hi2c    = &hi2c1;
  lcd1.address = LCD_ADDRESS;
  lcd_init(&lcd1);
  lcd_clear(&lcd1);

  /* ---- Relay & Buzzer ---- */
  Relay_Init(&relay1);
  Relay_Init(&relay2);
  HAL_GPIO_WritePin(CoiChip_GPIO_Port, CoiChip_Pin, GPIO_PIN_RESET);

  /* ---- System context ---- */
  SysCtrl_Init(&system_ctx);

  /* ---- Nút nhấn ---- */
  NodeButtons_Init();

  /* ---- Màn hình khởi động ---- */
  lcd_gotoxy(&lcd1, 0, 0); lcd_puts(&lcd1, "System Booting  ");
  lcd_gotoxy(&lcd1, 0, 1); lcd_puts(&lcd1, "STM32 IoT Node  ");

  /* ---- Bắt đầu nhận LoRa qua UART interrupt ---- */
  LoraNode_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */

    /* 1. Quét nút nhấn & cập nhật context */
    NodeButtons_Scan(&system_ctx);

    /* 2. Chạy state machine: đọc cảm biến -> điều khiển -> màn hình */
    SysCtrl_Run(&system_ctx);

    /* 3. Xử lý lệnh từ gateway (set bởi ISR) */
    /* Gateway gửi cấu hình mới (binary frame) */
      if (flag_rx_config != 0U)
      {
          flag_rx_config = 0U;
          SysCtrl_ApplyConfig(&system_ctx, &rx_config_shadow);
      }
    /* Gateway yêu cầu gửi dữ liệu ("senddata") */
    if (flag_send_data != 0U)
    {
        flag_send_data = 0U;
        if (flag_rx_config != 0U)
        {
            flag_rx_config = 0U;
            SysCtrl_ApplyConfig(&system_ctx, &rx_config_shadow);
        }
        uint8_t relayStatus = (uint8_t)((system_ctx.relay1_on ? 0x01U : 0x00U) |
                                        (system_ctx.relay2_on ? 0x02U : 0x00U));
        LoraNode_SendSensorFrame(
            system_ctx.temperature,
            system_ctx.humidity,
            system_ctx.mq2_adc,
            system_ctx.ldr_adc,
            system_ctx.temp_threshold,
            system_ctx.gas_threshold,
            relayStatus,
            system_ctx.alert_active,
            system_ctx.auto_mode
        );
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL         = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection    = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief ADC1 Initialization Function (MQ-2, Channel 3)
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance                   = ADC1;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }

  sConfig.Channel      = ADC_CHANNEL_3;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief ADC2 Initialization Function (LDR, Channel 1)
  */
static void MX_ADC2_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc2.Instance                   = ADC2;
  hadc2.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode    = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion       = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK) { Error_Handler(); }

  sConfig.Channel      = ADC_CHANNEL_1;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief I2C1 Initialization Function (LCD)
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 100000;
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief TIM1 Initialization Function (DHT11 timing, 1 MHz)
  */
static void MX_TIM1_Init(void)
{
  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  htim1.Instance               = TIM1;
  htim1.Init.Prescaler         = 63;       /* 64 MHz / 64 = 1 MHz */
  htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim1.Init.Period            = 65535;
  htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) { Error_Handler(); }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief USART1 Initialization Function (LoRa, 9600 baud)
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 9600;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Output reset */
  HAL_GPIO_WritePin(GPIOA, Relay2_Pin | Relay1_Pin | GPIO_PIN_6 | GPIO_PIN_11, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, CoiChip_Pin, GPIO_PIN_RESET);

  /* PA0, PA2: ADC input (no pull) */
  GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA3, PA4, PA6, PA11: Output PP (Relay, DHT11 data lines init) */
  GPIO_InitStruct.Pin   = Relay2_Pin | Relay1_Pin | GPIO_PIN_6 | GPIO_PIN_11;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA7: Input (no pull) */
  GPIO_InitStruct.Pin  = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA12, PA15: Button input (pull-up) */
  GPIO_InitStruct.Pin  = Button1_Pin | Button2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB3: Button input (pull-up) */
  GPIO_InitStruct.Pin  = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB4 (Button4): Input (pull-up) */
  GPIO_InitStruct.Pin  = Button4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Button4_GPIO_Port, &GPIO_InitStruct);

  /* PB12 (CoiChip / Buzzer): Output PP */
  GPIO_InitStruct.Pin   = CoiChip_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CoiChip_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief UART Rx complete callback — chuyển tiếp sang lora_node.c
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    LoraNode_RxCpltHandler();
  }
}

/* USER CODE END 4 */

/**
  * @brief Error Handler
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
