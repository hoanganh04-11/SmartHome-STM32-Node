#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include "node_config.h"
#include "stm32f1xx_hal.h"

/* ============================================================
 * SystemContext_t — trạng thái toàn bộ hệ thống
 * ============================================================ */
typedef enum {
    SYSTEM_STATE_BOOT = 0,
    SYSTEM_STATE_READ_DHT,
    SYSTEM_STATE_READ_ANALOG,
    SYSTEM_STATE_SCAN_BUTTONS,
    SYSTEM_STATE_CONTROL_OUTPUTS,
    SYSTEM_STATE_UPDATE_SCREEN,
    SYSTEM_STATE_IDLE
} SystemState_t;

typedef struct {
    SystemState_t state;
    uint32_t tick_dht;
    uint32_t tick_analog;
    uint32_t tick_lcd;
    uint32_t tick_button;

    /* Dữ liệu cảm biến */
    uint8_t  dht_valid;
    float    temperature;
    float    humidity;
    uint16_t mq2_adc;
    uint16_t ldr_adc;

    /* Trạng thái điều khiển (cập nhật từ gateway hoặc nút nhấn) */
    uint8_t  relay1_on;
    uint8_t  relay2_on;
    uint8_t  buzzer_on;
    uint8_t  auto_mode;        /* 1 = tự động */

    /* Ngưỡng nhận từ gateway */
    uint16_t temp_threshold;   /* °C              */
    uint16_t gas_threshold;    /* ADC raw         */

    /* Cảnh báo */
    uint8_t  alert_active;     /* 1 = có cảnh báo */

    /* LCD */
    uint8_t  lcd_page;         /* 0 hoặc 1        */
} SystemContext_t;

/* ============================================================
 * ADC & TIM handles (khai báo trong main.c, extern ở đây)
 * ============================================================ */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim1;
extern I2C_HandleTypeDef hi2c1;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief Khởi tạo SystemContext với giá trị mặc định.
 */
void SysCtrl_Init(SystemContext_t *ctx);

/**
 * @brief Chạy một vòng state machine (gọi liên tục trong while(1)).
 */
void SysCtrl_Run(SystemContext_t *ctx);

/**
 * @brief Xử lý cấu hình nhận từ gateway (gọi khi flag_rx_config == 1).
 */
void SysCtrl_ApplyConfig(SystemContext_t *ctx, const SensorData_t *cfg);

#endif /* SYSTEM_CONTROL_H */
