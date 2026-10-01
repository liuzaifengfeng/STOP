#ifndef CONTROLLER_SAFETY_H
#define CONTROLLER_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t controller_safety_start(bool radio_ready);
bool controller_safety_on_radio(const uint8_t *data, size_t len);
void controller_safety_set_maintenance(bool active);
/* 永久停机，最多 3 次请求回执；true 表示对端报告驱动关闭且 VOUT <2V。 */
bool controller_safety_shutdown(void);
/* 联调专用：发送 STOP 后暂停安全发射 10 秒，让 E22 接收反向诊断包。 */
esp_err_t controller_safety_begin_receive_test(void);
bool controller_safety_receive_test_active(void);
/* bit0=已配对，bit1=急停触点正常，bit2=维护停机；peer 为 BLE MAC。 */
uint8_t controller_safety_pair_status(uint8_t peer[6]);

#endif
