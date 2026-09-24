#ifndef STOP_PROTOCOL_H
#define STOP_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STOP_PROTOCOL_VERSION       1U
#define STOP_FRAME_HEADER_LEN       16U
#define STOP_FRAME_TRAILER_LEN      4U
#define STOP_FRAME_MIN_LEN          (STOP_FRAME_HEADER_LEN + STOP_FRAME_TRAILER_LEN)
#define STOP_FRAME_MAX_PAYLOAD      492U

#define STOP_FLAG_REQUEST           (1U << 0)
#define STOP_FLAG_RESPONSE          (1U << 1)
#define STOP_FLAG_EVENT             (1U << 2)
#define STOP_FLAG_ACK               (1U << 3)
#define STOP_FLAG_ERROR             (1U << 4)

typedef enum {
    STOP_MSG_DEVICE_INFO_GET = 0x01,
    STOP_MSG_DEVICE_STATUS_GET = 0x02,
    STOP_MSG_DEVICE_STATUS_EVENT = 0x03,
    STOP_MSG_CONFIG_GET = 0x10,
    STOP_MSG_CONFIG_SET = 0x11,
    STOP_MSG_CONFIG_RESULT = 0x12,
    STOP_MSG_RADIO_SEND = 0x20,
    STOP_MSG_RADIO_SEND_RESULT = 0x21,
    STOP_MSG_RADIO_RX_EVENT = 0x22,
    STOP_MSG_OTA_BEGIN = 0x30,
    STOP_MSG_OTA_DATA = 0x31,
    STOP_MSG_OTA_QUERY = 0x32,
    STOP_MSG_OTA_END = 0x33,
    STOP_MSG_OTA_ABORT = 0x34,
    STOP_MSG_OTA_STATUS = 0x35,
    STOP_MSG_OTA_RESULT = 0x36,
    STOP_MSG_LOG_EVENT = 0x40,
    STOP_MSG_ERROR = 0x7f,
} stop_message_type_t;

typedef enum {
    STOP_ERROR_OK = 0x0000,
    STOP_ERROR_INVALID_FRAME = 0x0001,
    STOP_ERROR_UNSUPPORTED_VERSION = 0x0002,
    STOP_ERROR_UNSUPPORTED_MESSAGE = 0x0003,
    STOP_ERROR_INVALID_ARGUMENT = 0x0004,
    STOP_ERROR_NOT_AUTHENTICATED = 0x0005,
    STOP_ERROR_NOT_AUTHORIZED = 0x0006,
    STOP_ERROR_BUSY = 0x0007,
    STOP_ERROR_INVALID_STATE = 0x0008,
    STOP_ERROR_QUEUE_FULL = 0x0009,
    STOP_ERROR_TIMEOUT = 0x000a,
    STOP_ERROR_CRC = 0x000b,
    STOP_ERROR_OFFSET_MISMATCH = 0x000c,
    STOP_ERROR_IMAGE_REJECTED = 0x000d,
    STOP_ERROR_SAFETY_LOCKOUT = 0x000e,
    STOP_ERROR_INTERNAL = 0x00ff,
} stop_error_t;

typedef struct {
    uint8_t message_type;
    uint8_t flags;
    uint16_t request_id;
    uint32_t sequence;
    const uint8_t *payload;
    uint16_t payload_len;
} stop_frame_view_t;

esp_err_t stop_frame_decode(const uint8_t *data, size_t len,
                            stop_frame_view_t *frame, stop_error_t *protocol_error);

esp_err_t stop_frame_encode(uint8_t message_type, uint8_t flags,
                            uint16_t request_id, uint32_t sequence,
                            const void *payload, uint16_t payload_len,
                            uint8_t *output, size_t output_capacity,
                            size_t *output_len);

uint16_t stop_crc16_ccitt_false(const uint8_t *data, size_t len);
uint32_t stop_crc32_iso_hdlc(const uint8_t *data, size_t len);

uint16_t stop_read_le16(const uint8_t *data);
uint32_t stop_read_le32(const uint8_t *data);
void stop_write_le16(uint8_t *data, uint16_t value);
void stop_write_le32(uint8_t *data, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
