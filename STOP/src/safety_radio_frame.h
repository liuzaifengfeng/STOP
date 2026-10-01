#ifndef SAFETY_RADIO_FRAME_H
#define SAFETY_RADIO_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAFETY_RADIO_VERSION 1U
#define SAFETY_RADIO_BASE_LEN 33U
#define SAFETY_RADIO_PAIR_LEN 49U
#define SAFETY_RADIO_KEY_LEN 16U

enum {
    SAFETY_RADIO_PAIR_REQUEST = 1,
    SAFETY_RADIO_PAIR_ACCEPT = 2,
    SAFETY_RADIO_HEARTBEAT = 3,
    SAFETY_RADIO_STOP_REQUEST = 4,
    SAFETY_RADIO_STOP_ACK = 5,
};

/* ACK 只报告停机，不授予任何使能许可。 */
enum {
    SAFETY_RADIO_ACK_DRIVER_OFF = 0, /* 已撤销 GPIO21，使输出电压尚未确认 */
    SAFETY_RADIO_ACK_VERIFIED_OFF = 1, /* 已撤销 GPIO21，INA226 输出低于 2V */
};

/* E22 载荷内部格式，所有多字节数为小端。BLE 的 STOP 帧完全不变。 */
typedef struct {
    uint8_t type;
    uint8_t source[6];
    uint8_t destination[6];
    uint32_t boot_id;
    uint32_t sequence;
    uint8_t state; /* 心跳 0=急停/1=正常；STOP_REQUEST 必须为 0；ACK 见上方枚举 */
    uint8_t key[SAFETY_RADIO_KEY_LEN]; /* 仅配对确认帧使用 */
} safety_radio_frame_t;

size_t safety_radio_encode(const safety_radio_frame_t *frame,
                           const uint8_t key[SAFETY_RADIO_KEY_LEN], uint8_t *out,
                           size_t capacity);
bool safety_radio_decode(const uint8_t *data, size_t len,
                         const uint8_t key[SAFETY_RADIO_KEY_LEN],
                         safety_radio_frame_t *frame);

/* 解码认证成功后，再校验回执对应的设备和本次请求。 */
bool safety_radio_stop_ack_matches(const safety_radio_frame_t *frame,
                                   const uint8_t peer[6], const uint8_t self[6],
                                   uint32_t boot_id, uint32_t sequence);

#endif
