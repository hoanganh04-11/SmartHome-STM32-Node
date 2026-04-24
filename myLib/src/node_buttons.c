#include "node_buttons.h"
#include "lora_node.h"     /* flag_send_data */

/* ============================================================
 * Button handles
 * ============================================================ */
Button_Typedef button_mode;
Button_Typedef button_relay1;
Button_Typedef button_relay2;
Button_Typedef button_view;

/* ============================================================
 * Event flags — set bởi callback (ISR-safe volatile)
 * ============================================================ */
volatile uint8_t event_toggle_mode     = 0U;
volatile uint8_t event_toggle_relay1   = 0U;
volatile uint8_t event_toggle_relay2   = 0U;
volatile uint8_t event_toggle_lcd_page = 0U;

/* ============================================================
 * HELPER: init trạng thái đọc ban đầu của nút
 * ============================================================ */
static void InitButtonRuntime(Button_Typedef *btn)
{
    uint8_t current = HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_PIN);
    btn->btn_current = current;
    btn->btn_last    = current;
    btn->btn_filter  = current;
}

/* ============================================================
 * NodeButtons_Init
 * Gán chân GPIO và khởi tạo 4 nút nhấn
 *   button_mode   : PA12 — toggle Auto/Manual
 *   button_relay1 : PA15 — toggle Relay 1 (chỉ hoạt động khi manual)
 *   button_relay2 : PB3  — toggle Relay 2 (chỉ hoạt động khi manual)
 *   button_view   : PB4  — chuyển trang LCD
 * ============================================================ */
void NodeButtons_Init(void)
{
    button_init(&button_mode,   GPIOA, GPIO_PIN_12);
    button_init(&button_relay1, GPIOA, GPIO_PIN_15);
    button_init(&button_relay2, GPIOB, GPIO_PIN_3);
    button_init(&button_view,   GPIOB, GPIO_PIN_4);

    InitButtonRuntime(&button_mode);
    InitButtonRuntime(&button_relay1);
    InitButtonRuntime(&button_relay2);
    InitButtonRuntime(&button_view);
}

/* ============================================================
 * NodeButtons_Scan
 * Gọi trong mỗi vòng lặp chính — quét nút và áp dụng sự kiện
 * ============================================================ */
void NodeButtons_Scan(SystemContext_t *ctx)
{
    /* ---- Debounce & detect ---- */
    button_handle(&button_mode);
    button_handle(&button_relay1);
    button_handle(&button_relay2);
    button_handle(&button_view);

    /* ---- Xử lý event: toggle Auto/Manual ---- */
    if (event_toggle_mode != 0U)
    {
        ctx->auto_mode ^= 1U;
        event_toggle_mode = 0U;
        flag_send_data = 1U;    /* Gửi trạng thái mới lên ESP32 */
    }

    /* ---- Xử lý event: toggle Relay 1 (chỉ khi manual) ---- */
    if ((ctx->auto_mode == 0U) && (event_toggle_relay1 != 0U))
    {
        ctx->relay1_on ^= 1U;
        flag_send_data = 1U;    /* Gửi trạng thái mới lên ESP32 */
    }
    event_toggle_relay1 = 0U;

    /* ---- Xử lý event: toggle Relay 2 (chỉ khi manual) ---- */
    if ((ctx->auto_mode == 0U) && (event_toggle_relay2 != 0U))
    {
        ctx->relay2_on ^= 1U;
        flag_send_data = 1U;    /* Gửi trạng thái mới lên ESP32 */
    }
    event_toggle_relay2 = 0U;

    /* ---- Xử lý event: chuyển trang LCD ---- */
    if (event_toggle_lcd_page != 0U)
    {
        ctx->lcd_page ^= 1U;
        event_toggle_lcd_page = 0U;
        flag_send_data = 1U;    /* Gửi trạng thái mới lên ESP32 */
    }
}

/* ============================================================
 * BUTTON CALLBACKS — gọi bởi thư viện Button khi phát hiện sự kiện
 * ============================================================ */

/* Short press */
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

/* Long press — giữ nguyên để mở rộng sau nếu cần */
void btn_press_timeout_callback(Button_Typedef *ButtonX)
{
    (void)ButtonX;
}
