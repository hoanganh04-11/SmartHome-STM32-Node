#include "lora_node.h"
#include <string.h>

/* ============================================================
 * LORA RX PARSER STATE MACHINE
 * ============================================================ */
typedef enum {
    LORA_PARSE_IDLE = 0,
    LORA_PARSE_BINARY,
    LORA_PARSE_TEXT
} LoraParseState_t;

#define LORA_TEXT_BUF_SIZE  32U

/* ---- Parser state (private) ---- */
static volatile LoraParseState_t s_parse_state = LORA_PARSE_IDLE;
static uint8_t  s_rx_buf[sizeof(SensorFrame_t)];
static uint8_t  s_rx_idx  = 0U;
static char     s_text_buf[LORA_TEXT_BUF_SIZE];
static uint8_t  s_text_idx = 0U;

/* ---- Public receive byte buffer (dùng bởi HAL_UART_Receive_IT) ---- */
volatile uint8_t lora_rx_byte = 0U;

/* ---- Public flags ---- */
volatile uint8_t flag_send_data   = 0U;
volatile uint8_t flag_rx_config   = 0U;
SensorData_t     rx_config_shadow = {0};

/* ============================================================
 * HELPER: XOR checksum
 * ============================================================ */
static uint8_t CalcXor(const uint8_t *data, uint16_t len)
{
    uint8_t cs = 0U;
    uint16_t i;
    for (i = 0U; i < len; i++)
    {
        cs ^= data[i];
    }
    return cs;
}

/* ============================================================
 * LoraNode_Init
 * ============================================================ */
void LoraNode_Init(void)
{
    s_parse_state = LORA_PARSE_IDLE;
    s_rx_idx      = 0U;
    s_text_idx    = 0U;
    flag_send_data = 0U;
    flag_rx_config = 0U;

    /* Bắt đầu nhận interrupt byte-by-byte */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&lora_rx_byte, 1U);
}

/* ============================================================
 * LoraNode_SendSensorFrame
 * Đóng gói SensorData_t và gửi frame LoRa lên gateway
 * ============================================================ */
void LoraNode_SendSensorFrame(float temperature, float humidity,
                               uint16_t gasValue, uint16_t lightValue,
                               uint16_t tempThreshold, uint16_t gasThreshold,
                               uint8_t relayStatus, uint8_t alertStatus,
                               uint8_t autoMode)
{
    SensorFrame_t frame;

    frame.startByte             = FRAME_START_BYTE;
    frame.payload.temperature   = temperature;
    frame.payload.humidity      = humidity;
    frame.payload.gasValue      = gasValue;
    frame.payload.lightValue    = lightValue;
    frame.payload.tempThreshold = tempThreshold;
    frame.payload.gasThreshold  = gasThreshold;
    frame.payload.relayStatus   = relayStatus;
    frame.payload.alertStatus   = alertStatus;
    frame.payload.autoMode      = autoMode;
    frame.checksum = CalcXor((const uint8_t *)&frame.payload,
                              (uint16_t)sizeof(frame.payload));
    frame.endByte               = FRAME_END_BYTE;

    HAL_UART_Transmit(&huart1, (uint8_t *)&frame, (uint16_t)sizeof(frame), 200U);
}

/* ============================================================
 * LoraNode_RxCpltHandler
 * Gọi từ HAL_UART_RxCpltCallback() trong main.c
 *
 * Luồng phân loại:
 *   Byte 0 == 0xAA  -> binary SensorFrame_t  -> flag_rx_config
 *   Byte 0 in [0x20,0x7E] -> text command    -> flag_send_data nếu "senddata"
 * ============================================================ */
void LoraNode_RxCpltHandler(void)
{
    uint8_t b = (uint8_t)lora_rx_byte;

    switch (s_parse_state)
    {
        /* ---- IDLE: chờ byte đầu tiên ---- */
        case LORA_PARSE_IDLE:
            if (b == FRAME_START_BYTE)
            {
                s_rx_idx = 0U;
                s_rx_buf[s_rx_idx++] = b;
                s_parse_state = LORA_PARSE_BINARY;
            }
            else if ((b >= 0x20U) && (b <= 0x7EU))
            {
                s_text_idx = 0U;
                s_text_buf[s_text_idx++] = (char)b;
                s_parse_state = LORA_PARSE_TEXT;
            }
            /* Byte khác (noise) -> ở lại IDLE */
            break;

        /* ---- BINARY: tích lũy bytes đủ sizeof(SensorFrame_t) ---- */
        case LORA_PARSE_BINARY:
            if (s_rx_idx < (uint8_t)sizeof(s_rx_buf))
            {
                s_rx_buf[s_rx_idx++] = b;
            }

            if (s_rx_idx >= (uint8_t)sizeof(SensorFrame_t))
            {
                /* Frame đủ -> validate */
                SensorFrame_t frame;
                memcpy(&frame, s_rx_buf, sizeof(SensorFrame_t));

                if ((frame.startByte == FRAME_START_BYTE) &&
                    (frame.endByte   == FRAME_END_BYTE))
                {
                    uint8_t cs = CalcXor((const uint8_t *)&frame.payload,
                                          (uint16_t)sizeof(frame.payload));
                    if (cs == frame.checksum)
                    {
                        rx_config_shadow = frame.payload;
                        flag_rx_config   = 1U;
                    }
                }

                s_rx_idx      = 0U;
                s_parse_state = LORA_PARSE_IDLE;
            }
            break;

        /* ---- TEXT: tích lũy ký tự đến '\n' hoặc '\r' ---- */
        case LORA_PARSE_TEXT:
            if ((b == '\n') || (b == '\r'))
            {
                s_text_buf[s_text_idx] = '\0';

                /* Nhận dạng lệnh "senddata" */
                if (strncmp(s_text_buf, SENDDATA_CMD,
                            sizeof(SENDDATA_CMD) - 1U) == 0)
                {
                    flag_send_data = 1U;
                }

                s_text_idx    = 0U;
                s_parse_state = LORA_PARSE_IDLE;
            }
            else if ((b >= 0x20U) && (b <= 0x7EU))
            {
                if (s_text_idx < (LORA_TEXT_BUF_SIZE - 1U))
                {
                    s_text_buf[s_text_idx++] = (char)b;
                }
                else
                {
                    /* Chuỗi quá dài -> reset */
                    s_text_idx    = 0U;
                    s_parse_state = LORA_PARSE_IDLE;
                }
            }
            else
            {
                /* Ký tự lạ -> reset */
                s_text_idx    = 0U;
                s_parse_state = LORA_PARSE_IDLE;
            }
            break;

        default:
            s_parse_state = LORA_PARSE_IDLE;
            break;
    }

    /* Tiếp tục nhận byte tiếp theo */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&lora_rx_byte, 1U);
}
