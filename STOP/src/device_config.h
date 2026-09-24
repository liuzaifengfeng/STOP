#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define DEVICE_ALIAS_MAX_LEN 31U

typedef enum {
    DEVICE_CONFIG_ALIAS = 0x0001,
    DEVICE_CONFIG_STATUS_PERIOD_MS = 0x0002,
    DEVICE_CONFIG_RADIO_TX_TIMEOUT_MS = 0x0003,
} device_config_key_t;

typedef enum {
    DEVICE_CONFIG_TYPE_U32 = 0x02,
    DEVICE_CONFIG_TYPE_STRING = 0x03,
} device_config_type_t;

typedef struct {
    char alias[DEVICE_ALIAS_MAX_LEN + 1U];
    uint32_t status_period_ms;
    uint32_t radio_tx_timeout_ms;
} device_config_t;

esp_err_t device_config_init(void);
void device_config_get(device_config_t *config);
esp_err_t device_config_apply_tlv(const uint8_t *payload, size_t payload_len);
esp_err_t device_config_encode_tlv(const uint8_t *requested_keys, size_t key_count,
                                   uint8_t *output, size_t capacity,
                                   size_t *output_len);

#endif
