#ifndef RECEIVER_SAFETY_H
#define RECEIVER_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

enum {
    RECEIVER_UNPAIRED = 1,
    RECEIVER_TRIPPED = 2,
    RECEIVER_ARMED = 3,
    RECEIVER_LINK_LOST = 4,
    RECEIVER_HARDWARE_FAULT = 5,
    RECEIVER_PAIRING = 6,
};

/* 临时 BLE 调试：使能保留全部安全条件；false 立即断开。 */
esp_err_t receiver_safety_debug_output(bool enable);
esp_err_t receiver_safety_start(bool radio_ready);
bool receiver_safety_on_radio(const uint8_t *data, size_t len);
uint8_t receiver_safety_state(void);
void receiver_safety_hardware_fault(void);
/* bit0=已配对，bit1=远端允许，bit2=有效心跳未超时，bit3=本地键按下，bit4=输出已接通。 */
uint8_t receiver_safety_pair_status(uint8_t peer[6]);
/* 输出断开且无线健康时打开 30 秒配对窗口；BLE 与本地长按共用。 */
esp_err_t receiver_safety_open_pair_window(void);
/* 稳定按下后的持续时间，供状态灯显示长按进度。 */
uint32_t receiver_safety_key_hold_ms(void);

#endif
