#include "device_config.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "stop_protocol.h"

#define CONFIG_NAMESPACE "stop_cfg"
#define DEFAULT_ALIAS "STOP-C6"
#define DEFAULT_STATUS_PERIOD_MS 5000U
#define DEFAULT_RADIO_TX_TIMEOUT_MS 1000U

static SemaphoreHandle_t s_mutex;
static device_config_t s_config;

static bool valid_config(const device_config_t *config)
{
    size_t alias_len = strnlen(config->alias, sizeof(config->alias));
    return alias_len > 0U && alias_len <= DEVICE_ALIAS_MAX_LEN &&
           config->status_period_ms >= 1000U && config->status_period_ms <= 60000U &&
           config->radio_tx_timeout_ms >= 100U &&
           config->radio_tx_timeout_ms <= 5000U;
}

esp_err_t device_config_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_config, 0, sizeof(s_config));
    memcpy(s_config.alias, DEFAULT_ALIAS, sizeof(DEFAULT_ALIAS));
    s_config.status_period_ms = DEFAULT_STATUS_PERIOD_MS;
    s_config.radio_tx_timeout_ms = DEFAULT_RADIO_TX_TIMEOUT_MS;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    device_config_t loaded = s_config;
    size_t alias_capacity = sizeof(loaded.alias);
    (void)nvs_get_str(handle, "alias", loaded.alias, &alias_capacity);
    (void)nvs_get_u32(handle, "status_ms", &loaded.status_period_ms);
    (void)nvs_get_u32(handle, "radio_ms", &loaded.radio_tx_timeout_ms);
    nvs_close(handle);
    if (valid_config(&loaded)) {
        s_config = loaded;
    }
    return ESP_OK;
}

void device_config_get(device_config_t *config)
{
    if (config == NULL || s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *config = s_config;
    xSemaphoreGive(s_mutex);
}

static esp_err_t parse_entry(device_config_t *candidate, uint16_t key,
                             uint8_t type, const uint8_t *value, uint16_t len)
{
    switch (key) {
    case DEVICE_CONFIG_ALIAS:
        if (type != DEVICE_CONFIG_TYPE_STRING || len == 0U ||
            len > DEVICE_ALIAS_MAX_LEN || memchr(value, '\0', len) != NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(candidate->alias, value, len);
        candidate->alias[len] = '\0';
        return ESP_OK;
    case DEVICE_CONFIG_STATUS_PERIOD_MS:
        if (type != DEVICE_CONFIG_TYPE_U32 || len != 4U) {
            return ESP_ERR_INVALID_ARG;
        }
        candidate->status_period_ms = stop_read_le32(value);
        return ESP_OK;
    case DEVICE_CONFIG_RADIO_TX_TIMEOUT_MS:
        if (type != DEVICE_CONFIG_TYPE_U32 || len != 4U) {
            return ESP_ERR_INVALID_ARG;
        }
        candidate->radio_tx_timeout_ms = stop_read_le32(value);
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t device_config_apply_tlv(const uint8_t *payload, size_t payload_len)
{
    if (payload == NULL || payload_len < 1U || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t count = payload[0];
    size_t offset = 1U;
    device_config_t candidate;
    device_config_get(&candidate);

    for (uint8_t i = 0; i < count; ++i) {
        if (offset + 5U > payload_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        uint16_t key = stop_read_le16(payload + offset);
        uint8_t type = payload[offset + 2U];
        uint16_t len = stop_read_le16(payload + offset + 3U);
        offset += 5U;
        if (offset + len > payload_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        esp_err_t err = parse_entry(&candidate, key, type, payload + offset, len);
        if (err != ESP_OK) {
            return err;
        }
        offset += len;
    }
    if (count == 0U || offset != payload_len || !valid_config(&candidate)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, "alias", candidate.alias);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, "status_ms", candidate.status_period_ms);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, "radio_ms", candidate.radio_tx_timeout_ms);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_config = candidate;
        xSemaphoreGive(s_mutex);
    }
    return err;
}

static esp_err_t append_entry(uint16_t key, const device_config_t *config,
                              uint8_t *output, size_t capacity, size_t *offset)
{
    const uint8_t *value = NULL;
    uint8_t scalar[4];
    uint8_t type;
    uint16_t len;
    switch (key) {
    case DEVICE_CONFIG_ALIAS:
        type = DEVICE_CONFIG_TYPE_STRING;
        value = (const uint8_t *)config->alias;
        len = (uint16_t)strlen(config->alias);
        break;
    case DEVICE_CONFIG_STATUS_PERIOD_MS:
        type = DEVICE_CONFIG_TYPE_U32;
        stop_write_le32(scalar, config->status_period_ms);
        value = scalar;
        len = sizeof(scalar);
        break;
    case DEVICE_CONFIG_RADIO_TX_TIMEOUT_MS:
        type = DEVICE_CONFIG_TYPE_U32;
        stop_write_le32(scalar, config->radio_tx_timeout_ms);
        value = scalar;
        len = sizeof(scalar);
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (*offset + 5U + len > capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    stop_write_le16(output + *offset, key);
    output[*offset + 2U] = type;
    stop_write_le16(output + *offset + 3U, len);
    memcpy(output + *offset + 5U, value, len);
    *offset += 5U + len;
    return ESP_OK;
}

esp_err_t device_config_encode_tlv(const uint8_t *requested_keys, size_t key_count,
                                   uint8_t *output, size_t capacity,
                                   size_t *output_len)
{
    if (output == NULL || output_len == NULL || capacity < 1U ||
        (key_count != 0U && requested_keys == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint16_t all_keys[] = {DEVICE_CONFIG_ALIAS, DEVICE_CONFIG_STATUS_PERIOD_MS,
                                 DEVICE_CONFIG_RADIO_TX_TIMEOUT_MS};
    device_config_t snapshot;
    device_config_get(&snapshot);
    size_t offset = 1U;
    size_t count = key_count == 0U ? sizeof(all_keys) / sizeof(all_keys[0]) : key_count;
    if (count > UINT8_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < count; ++i) {
        uint16_t key = key_count == 0U ? all_keys[i] : stop_read_le16(requested_keys + i * 2U);
        esp_err_t err = append_entry(key, &snapshot, output, capacity, &offset);
        if (err != ESP_OK) {
            return err;
        }
    }
    output[0] = (uint8_t)count;
    *output_len = offset;
    return ESP_OK;
}
