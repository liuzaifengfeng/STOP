#ifndef RADIO_TRANSPORT_H
#define RADIO_TRANSPORT_H

#include <stdbool.h>
#include "E22-400t22s.h"

/* NVS 模式：0=E22，1=ESP-NOW，2=双通道。保存后重启生效。 */
#define RADIO_LINK_E22 0U
#define RADIO_LINK_ESPNOW 1U
#define RADIO_LINK_DUAL 2U

/* 保持现有 BLE 诊断包上限；ESP-NOW v1 也能容纳这个长度。 */
#define RADIO_MAX_PAYLOAD_LEN E22_MAX_PAYLOAD_LEN
#define RADIO_MODE_DUAL 6U
#define RADIO_MODE_ESPNOW 5U /* STATUS 的 radio_mode；0..4 保留原 E22 含义 */
typedef e22_rx_msg_t radio_rx_msg_t;

esp_err_t radio_transport_init(void);
esp_err_t radio_transport_send(const uint8_t *data, size_t len, TickType_t timeout);
esp_err_t radio_transport_receive(radio_rx_msg_t *msg, TickType_t timeout);
const char *radio_transport_name(void);
uint8_t radio_transport_mode(void);
bool radio_transport_e22_ready(void);

#endif
