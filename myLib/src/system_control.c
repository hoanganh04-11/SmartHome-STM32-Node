#include "system_control.h"
#include "LCD_I2C.h"
#include "DHT11.h"
#include "Relay.h"
#include "Mq2.h"
#include "LDR.h"
#include "lora_node.h"     /* flag_send_data */
#include <string.h>
#include <stdio.h>

/* ============================================================
 * HARDWARE INSTANCES (extern từ main.c)
 * ============================================================ */
extern DHT11_InitTypedef dht11_nodes[DHT_SENSOR_COUNT];
extern I2C_LCD_HandleTypedef lcd1;
extern Relay_HandleTypeDef   relay1;
extern Relay_HandleTypeDef   relay2;

/* ============================================================
 * HELPER
 * ============================================================ */
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
    memset(buf, ' ', 16U);
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
    HAL_GPIO_WritePin(CoiChip_GPIO_Port, CoiChip_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================
 * SysCtrl_Init
 * ============================================================ */
void SysCtrl_Init(SystemContext_t *ctx)
{
    memset(ctx, 0, sizeof(SystemContext_t));
    ctx->state          = SYSTEM_STATE_BOOT;
    ctx->auto_mode      = 1U;    /* Mặc định: tự động */
    ctx->temp_threshold = 35U;   /* °C  — đồng bộ với gateway default */
    ctx->gas_threshold  = 600U;  /* ADC — đồng bộ với gateway default */
}

/* ============================================================
 * SysCtrl_ApplyConfig
 * Áp dụng cấu hình nhận từ gateway (gọi khi flag_rx_config == 1)
 * ============================================================ */
void SysCtrl_ApplyConfig(SystemContext_t *ctx, const SensorData_t *cfg)
{
    /* Ngưỡng và chế độ — luôn cập nhật */
    ctx->temp_threshold = cfg->tempThreshold;
    ctx->gas_threshold  = cfg->gasThreshold;
    ctx->auto_mode      = cfg->autoMode ? 1U : 0U;

    /* Relay chỉ áp dụng ngay khi ở chế độ tay;
     * Nếu auto thì ControlOutputs() sẽ tự tính theo cảm biến */
    if (ctx->auto_mode == 0U)
    {
        ctx->relay1_on = (cfg->relayStatus & 0x01U) ? 1U : 0U;
        ctx->relay2_on = (cfg->relayStatus & 0x02U) ? 1U : 0U;
    }
}

/* ============================================================
 * ReadDhtSensor (private)
 * ============================================================ */
static void ReadDhtSensor(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_dht, DHT_READ_PERIOD_MS))
    {
        float    temp_sum   = 0.0f;
        float    hum_sum    = 0.0f;
        uint8_t  valid_count = 0U;
        uint8_t  i;

        for (i = 0U; i < (uint8_t)DHT_SENSOR_COUNT; i++)
        {
            if (readDHT11(&dht11_nodes[i]) != 0U)
            {
                temp_sum += (float)dht11_nodes[i].temperature;
                hum_sum  += (float)dht11_nodes[i].humidity;
                valid_count++;
            }
        }

        if (valid_count > 0U)
        {
            ctx->dht_valid   = 1U;
            ctx->temperature = temp_sum / (float)valid_count;
            ctx->humidity    = hum_sum  / (float)valid_count;
        }
        else
        {
            ctx->dht_valid = 0U;
        }
    }
}

/* ============================================================
 * ReadAnalogSensors (private)
 * ============================================================ */
static void ReadAnalogSensors(SystemContext_t *ctx)
{
    if (IsTimeElapsed(&ctx->tick_analog, ANALOG_READ_PERIOD_MS))
    {
        ctx->mq2_adc = MQ2_Read_ADC_Average(&hadc1, MQ2_SAMPLE_COUNT)*100/4095;
        ctx->ldr_adc = LDR_Read_ADC_Average(&hadc2, LDR_SAMPLE_COUNT)*100/4095;
    }
}

/* ============================================================
 * ControlOutputs (private)
 * ============================================================ */
static void ControlOutputs(SystemContext_t *ctx)
{
    /* ---- Lưu trạng thái cảnh báo trước đó để phát hiện cạnh ---- */
    uint8_t prev_alert = ctx->alert_active;

    /* ---- Tính cảnh báo gas với hysteresis ---- */
    uint16_t thr_on  = ctx->gas_threshold;
    uint16_t thr_off = (ctx->gas_threshold > 200U)
                       ? (ctx->gas_threshold - 200U) : 0U;

    if (ctx->mq2_adc >= thr_on)
    {
        ctx->alert_active = 1U;
    }
    else if (ctx->mq2_adc <= thr_off)
    {
        ctx->alert_active = 0U;
    }

    /* ---- Gửi dữ liệu ngay khi cảnh báo thay đổi trạng thái ---- */
    if (ctx->alert_active != prev_alert)
    {
        flag_send_data = 1U;
    }

    /* ---- Chế độ tự động ---- */
    if (ctx->auto_mode != 0U)
    {
        /* Relay 1: bật khi phát hiện gas vượt ngưỡng */
        ctx->relay1_on = (ctx->alert_active != 0U) ? 1U : 0U;

        /* Relay 2: bật khi nhiệt độ >= ngưỡng nhiệt, gửi dữ liệu khi thay đổi */
        uint8_t prev_r2 = ctx->relay2_on;
        ctx->relay2_on  = (ctx->temperature >= (float)ctx->temp_threshold) ? 1U : 0U;
        if (ctx->relay2_on != prev_r2)
        {
            flag_send_data = 1U;   /* Nhiệt độ vượt / về dưới ngưỡng */
        }
    }

    /* Còi: theo alert bất kể chế độ */
    ctx->buzzer_on = ctx->alert_active;

    /* Relay active-low: RELAY_OFF = GPIO_RESET = cuộn dây cấp điện = BẬT vật lý */
    Relay_SetState(&relay1, ctx->relay1_on ? RELAY_OFF : RELAY_ON);
    Relay_SetState(&relay2, ctx->relay2_on ? RELAY_OFF : RELAY_ON);
    Buzzer_Set(ctx->buzzer_on);
}

/* ============================================================
 * UpdateScreen (private)
 * ============================================================ */
static void UpdateScreen(SystemContext_t *ctx)
{
    if (!IsTimeElapsed(&ctx->tick_lcd, LCD_UPDATE_PERIOD_MS))
    {
        return;
    }

    char line1[17];
    char line2[17];

    if (ctx->lcd_page == 0U)
    {
        /* Trang 0: nhiệt độ & độ ẩm, gas & ánh sáng */
        if (ctx->dht_valid != 0U)
        {
            snprintf(line1, sizeof(line1),
                     "T:%.1fC H:%.0f%%",
                     ctx->temperature, ctx->humidity);
        }
        else
        {
            snprintf(line1, sizeof(line1), "DHT Error");
        }
        snprintf(line2, sizeof(line2),
                 "G:%4u L:%4u", ctx->mq2_adc, ctx->ldr_adc);
    }
    else
    {
        /* Trang 1: chế độ, relay, cảnh báo */
        snprintf(line1, sizeof(line1),
                 "%s R1:%u R2:%u",
                 (ctx->auto_mode != 0U) ? "AUTO" : "MAN ",
                 (unsigned)ctx->relay1_on,
                 (unsigned)ctx->relay2_on);
        snprintf(line2, sizeof(line2),
                 "ALT:%u GT:%4u",
                 (unsigned)ctx->alert_active,
                 ctx->gas_threshold);
    }

    LCD_PrintLine(0U, line1);
    LCD_PrintLine(1U, line2);
}

/* ============================================================
 * SysCtrl_Run — một vòng state machine
 * ============================================================ */
void SysCtrl_Run(SystemContext_t *ctx)
{
    switch (ctx->state)
    {
        case SYSTEM_STATE_BOOT:
            ctx->state = SYSTEM_STATE_READ_DHT;
            break;

        case SYSTEM_STATE_READ_DHT:
            ReadDhtSensor(ctx);
            ctx->state = SYSTEM_STATE_READ_ANALOG;
            break;

        case SYSTEM_STATE_READ_ANALOG:
            ReadAnalogSensors(ctx);
            ctx->state = SYSTEM_STATE_SCAN_BUTTONS;
            break;

        case SYSTEM_STATE_SCAN_BUTTONS:
            /* ScanButtons được xử lý riêng trong node_buttons.c,
             * state machine chỉ chuyển sang bước tiếp theo */
            ctx->state = SYSTEM_STATE_CONTROL_OUTPUTS;
            break;

        case SYSTEM_STATE_CONTROL_OUTPUTS:
            ControlOutputs(ctx);
            ctx->state = SYSTEM_STATE_UPDATE_SCREEN;
            break;

        case SYSTEM_STATE_UPDATE_SCREEN:
            UpdateScreen(ctx);
            ctx->state = SYSTEM_STATE_IDLE;
            break;

        case SYSTEM_STATE_IDLE:
        default:
            ctx->state = SYSTEM_STATE_READ_DHT;
            break;
    }
}
