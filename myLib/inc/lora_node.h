#ifndef LORA_NODE_H
#define LORA_NODE_H

#include "node_config.h"
#include "stm32f1xx_hal.h"

/* ============================================================
 * Cờ hành động — set trong ISR, xử lý trong main loop
 * ============================================================ */
extern volatile uint8_t flag_send_data;   /* Gateway yêu cầu gửi dữ liệu cảm biến */
extern volatile uint8_t flag_rx_config;   /* Nhận cấu hình mới từ gateway           */
extern SensorData_t     rx_config_shadow; /* Bản sao cấu hình nhận được             */

/* ============================================================
 * UART handle sử dụng để giao tiếp LoRa
 * (được khai báo trong main.c, extern ở đây)
 * ============================================================ */
extern UART_HandleTypeDef huart1;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief Khởi động nhận UART interrupt (gọi một lần trong main).
 */
void LoraNode_Init(void);

/**
 * @brief Đóng gói SensorData_t đầy đủ và gửi frame LoRa lên gateway.
 *        Dùng khi flag_send_data == 1.
 */
void LoraNode_SendSensorFrame(float temperature, float humidity,
                               uint16_t gasValue, uint16_t lightValue,
                               uint16_t tempThreshold, uint16_t gasThreshold,
                               uint8_t relayStatus, uint8_t alertStatus,
                               uint8_t autoMode);

/**
 * @brief UART RxCplt callback — gọi từ HAL_UART_RxCpltCallback() trong main.c.
 *        Xử lý parser 2 loại: text "senddata" và binary SensorFrame_t.
 */
void LoraNode_RxCpltHandler(void);

#endif /* LORA_NODE_H */
