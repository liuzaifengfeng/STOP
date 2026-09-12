#include "stop_protocol.h"

#include <string.h>

#include "ble_transport.h"

uint16_t stop_read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t stop_read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void stop_write_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

void stop_write_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

uint16_t stop_crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xffffU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? (uint16_t)((crc << 1) ^ 0x1021U)
                      : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint32_t stop_crc32_iso_hdlc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1) ^ 0xedb88320U : crc >> 1;
        }
    }
    return crc ^ 0xffffffffU;
}

esp_err_t stop_frame_decode(const uint8_t *data, size_t len,
                            stop_frame_view_t *frame, stop_error_t *protocol_error)
{
    if (protocol_error != NULL) {
        *protocol_error = STOP_ERROR_INVALID_FRAME;
    }
    if (data == NULL || frame == NULL || len < STOP_FRAME_MIN_LEN ||
        len > BLE_TRANSPORT_MAX_FRAME_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data[0] != 'S' || data[1] != 'T' || data[5] != STOP_FRAME_HEADER_LEN) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (data[2] != STOP_PROTOCOL_VERSION) {
        if (protocol_error != NULL) {
            *protocol_error = STOP_ERROR_UNSUPPORTED_VERSION;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t payload_len = stop_read_le16(data + 12);
    if (payload_len > STOP_FRAME_MAX_PAYLOAD ||
        len != STOP_FRAME_HEADER_LEN + payload_len + STOP_FRAME_TRAILER_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (stop_read_le16(data + 14) != stop_crc16_ccitt_false(data, 14)) {
        if (protocol_error != NULL) {
            *protocol_error = STOP_ERROR_CRC;
        }
        return ESP_ERR_INVALID_CRC;
    }
    if (stop_read_le32(data + STOP_FRAME_HEADER_LEN + payload_len) !=
        stop_crc32_iso_hdlc(data, STOP_FRAME_HEADER_LEN + payload_len)) {
        if (protocol_error != NULL) {
            *protocol_error = STOP_ERROR_CRC;
        }
        return ESP_ERR_INVALID_CRC;
    }

    frame->message_type = data[3];
    frame->flags = data[4];
    frame->request_id = stop_read_le16(data + 6);
    frame->sequence = stop_read_le32(data + 8);
    frame->payload = data + STOP_FRAME_HEADER_LEN;
    frame->payload_len = payload_len;
    if (protocol_error != NULL) {
        *protocol_error = STOP_ERROR_OK;
    }
    return ESP_OK;
}

esp_err_t stop_frame_encode(uint8_t message_type, uint8_t flags,
                            uint16_t request_id, uint32_t sequence,
                            const void *payload, uint16_t payload_len,
                            uint8_t *output, size_t output_capacity,
                            size_t *output_len)
{
    size_t frame_len = STOP_FRAME_HEADER_LEN + payload_len + STOP_FRAME_TRAILER_LEN;
    if (output == NULL || output_len == NULL ||
        (payload_len != 0U && payload == NULL) ||
        payload_len > STOP_FRAME_MAX_PAYLOAD || output_capacity < frame_len) {
        return ESP_ERR_INVALID_ARG;
    }

    output[0] = 'S';
    output[1] = 'T';
    output[2] = STOP_PROTOCOL_VERSION;
    output[3] = message_type;
    output[4] = flags;
    output[5] = STOP_FRAME_HEADER_LEN;
    stop_write_le16(output + 6, request_id);
    stop_write_le32(output + 8, sequence);
    stop_write_le16(output + 12, payload_len);
    stop_write_le16(output + 14, stop_crc16_ccitt_false(output, 14));
    if (payload_len != 0U) {
        memcpy(output + STOP_FRAME_HEADER_LEN, payload, payload_len);
    }
    stop_write_le32(output + STOP_FRAME_HEADER_LEN + payload_len,
                    stop_crc32_iso_hdlc(output, STOP_FRAME_HEADER_LEN + payload_len));
    *output_len = frame_len;
    return ESP_OK;
}
