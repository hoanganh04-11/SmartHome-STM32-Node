#ifndef NODE_BUTTONS_H
#define NODE_BUTTONS_H

#include "main.h"
#include "Button.h"
#include "system_control.h"

/* ============================================================
 * Button handles (extern — khai báo trong node_buttons.c,
 * dùng được ở main.c để init)
 * ============================================================ */
extern Button_Typedef button_mode;
extern Button_Typedef button_relay1;
extern Button_Typedef button_relay2;
extern Button_Typedef button_view;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief Khởi tạo 4 nút nhấn (gọi một lần trong main).
 */
void NodeButtons_Init(void);

/**
 * @brief Quét và xử lý sự kiện nút nhấn, cập nhật SystemContext.
 *        Gọi trong mỗi vòng lặp chính.
 */
void NodeButtons_Scan(SystemContext_t *ctx);

#endif /* NODE_BUTTONS_H */
