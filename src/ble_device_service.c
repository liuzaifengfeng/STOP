#include "ble_device_service.h"

#include <string.h>

#include "E22-400t22s.h"
#include "adc_monitor.h"
#include "ble_transport.h"
#include "device_config.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_service.h"
#include "stop_protocol.h"

#define SERVICE_TASK_STACK       4096U
#define SERVICE_TASK_PRIORITY    5U
#define RESPONSE_BUFFER_SIZE     BLE_TRANSPORT_MAX_FRAME_LEN

#define CAP_CONFIG               (1UL << 0)
#define CAP_RADIO_DIAGNOSTIC     (1UL << 1)
#define CAP_BLE_OTA              (1UL << 2)
#define CAP_OTA_SHA256           (1UL << 3)
#define CAP_OTA_SIGNED           (1UL << 4)

static const char *TAG = "BLE_SERVICE";
static bool s_radio_ready;
static uint32_t s_tx_sequence;
static portMUX_TYPE s_sequence_lock = portMUX_INITIALIZER_UNLOCKED;

static uint32_t next_sequence(void)
{
    portENTER_CRITICAL(&s_sequence_lock);
    uint32_t result = ++s_tx_sequence;
    portEXIT_CRITICAL(&s_sequence_lock);
    return result;
}

static esp_err_t send_frame(bool bulk_ack, uint8_t message_type, uint8_t flags,
                            uint16_t request_id, const void *payload,
                            uint16_t payload_len)
{
    uint8_t frame[RESPONSE_BUFFER_SIZE];
    size_t frame_len;
    esp_err_t err = stop_frame_encode(message_type, flags, request_id,
                                      next_sequence(), payload, payload_len,
                                      frame, sizeof(frame), &frame_len);
    if (err != ESP_OK) {
        return err;
    }
    return bulk_ack ? ble_transport_send_bulk_ack(frame, frame_len)
                    : ble_transport_send_event(frame, frame_len);
}

static void send_error(uint16_t request_id, stop_error_t error, bool bulk_ack)
{
    uint8_t payload[3];
    stop_write_le16(payload, (uint16_t)error);
    payload[2] = 0U;
    esp_err_t err = send_frame(bulk_ack, STOP_MSG_ERROR,
                               STOP_FLAG_RESPONSE | STOP_FLAG_ERROR,
                               request_id, payload, sizeof(payload));
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Unable to send protocol error %u: %s", error,
                 esp_err_to_name(err));
    }
}

static void send_response(uint8_t message_type, uint16_t request_id,
                          const void *payload, uint16_t payload_len)
{
    esp_err_t err = send_frame(false, message_type, STOP_FLAG_RESPONSE,
                               request_id, payload, payload_len);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Unable to send response 0x%02x: %s", message_type,
                 esp_err_to_name(err));
    }
}

static uint16_t encode_device_info(uint8_t *output, size_t capacity)
{
    const esp_app_desc_t *app = esp_app_get_description();
    size_t version_len = strnlen(app->version, sizeof(app->version));
    if (version_len > 31U || capacity < 16U + version_len) {
        return 0U;
    }
    output[0] = STOP_PROTOCOL_VERSION;
    uint32_t capabilities = CAP_CONFIG | CAP_RADIO_DIAGNOSTIC | CAP_BLE_OTA |
                            CAP_OTA_SHA256;
#if CONFIG_SECURE_BOOT
    capabilities |= CAP_OTA_SIGNED;
#endif
    stop_write_le32(output + 1, capabilities);
    stop_write_le16(output + 5, STOP_PRODUCT_ID_BUTTON_BOX);
    stop_write_le16(output + 7, STOP_HARDWARE_REVISION);
    output[9] = (uint8_t)version_len;
    memcpy(output + 10, app->version, version_len);
    uint8_t mac[6] = {0};
    (void)esp_read_mac(mac, ESP_MAC_BT);
    memcpy(output + 10 + version_len, mac, sizeof(mac));
    return (uint16_t)(16U + version_len);
}

static uint16_t encode_device_status(uint8_t *output, size_t capacity)
{
    if (capacity < 20U) {
        return 0U;
    }
    BatteryInfo battery = {0};
    bool battery_valid = get_battery_info(&battery);
    uint32_t voltage_mv = battery_valid && battery.voltage_v > 0.0f
                              ? (uint32_t)(battery.voltage_v * 1000.0f)
                              : 0U;
    if (voltage_mv > UINT16_MAX) {
        voltage_mv = UINT16_MAX;
    }
    ota_service_status_t ota;
    ota_service_get_status(&ota);

    stop_write_le32(output, (uint32_t)(esp_timer_get_time() / 1000000LL));
    stop_write_le16(output + 4, (uint16_t)voltage_mv);
    output[6] = battery_valid && battery.soc >= 0 && battery.soc <= 100
                    ? (uint8_t)battery.soc
                    : 0xffU;
    output[7] = s_radio_ready ? 1U : 0U;
    output[8] = (uint8_t)e22_get_mode();
    output[9] = ble_transport_is_connected() ? 1U : 0U;
    stop_write_le16(output + 10, ble_transport_get_mtu());
    output[12] = (uint8_t)ota.state;
    stop_write_le16(output + 13, (uint16_t)ota.last_error);
    stop_write_le32(output + 15, ota.expected_offset);
    output[19] = 0U; /* Safety state unavailable until safety_manager exists. */
    return 20U;
}

static void publish_status(void)
{
    uint8_t payload[32];
    uint16_t payload_len = encode_device_status(payload, sizeof(payload));
    uint8_t frame[64];
    size_t frame_len;
    if (payload_len != 0U &&
        stop_frame_encode(STOP_MSG_DEVICE_STATUS_EVENT, STOP_FLAG_EVENT, 0,
                          next_sequence(), payload, payload_len, frame,
                          sizeof(frame), &frame_len) == ESP_OK) {
        esp_err_t err = ble_transport_publish_status(frame, frame_len);
        if (err != ESP_OK && err != ESP_ERR_INVALID_SIZE) {
            ESP_LOGW(TAG, "Status publish failed: %s", esp_err_to_name(err));
        }
    }
}

static stop_error_t config_error_from_esp(esp_err_t err)
{
    if (err == ESP_ERR_NOT_SUPPORTED) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    return STOP_ERROR_INTERNAL;
}

static void handle_config_get(const stop_frame_view_t *frame)
{
    if (frame->payload_len < 1U) {
        send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
        return;
    }
    uint8_t count = frame->payload[0];
    if (frame->payload_len != 1U + ((uint16_t)count * 2U)) {
        send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
        return;
    }
    uint8_t payload[128];
    size_t payload_len;
    esp_err_t err = device_config_encode_tlv(frame->payload + 1, count,
                                             payload, sizeof(payload), &payload_len);
    if (err != ESP_OK) {
        send_error(frame->request_id, config_error_from_esp(err), false);
        return;
    }
    send_response(STOP_MSG_CONFIG_RESULT, frame->request_id, payload,
                  (uint16_t)payload_len);
}

static void handle_config_set(const stop_frame_view_t *frame)
{
    if (ota_service_is_active()) {
        send_error(frame->request_id, STOP_ERROR_BUSY, false);
        return;
    }
    esp_err_t err = device_config_apply_tlv(frame->payload, frame->payload_len);
    if (err != ESP_OK) {
        send_error(frame->request_id, config_error_from_esp(err), false);
        return;
    }
    uint8_t payload[128];
    size_t payload_len;
    err = device_config_encode_tlv(NULL, 0, payload, sizeof(payload), &payload_len);
    if (err != ESP_OK) {
        send_error(frame->request_id, STOP_ERROR_INTERNAL, false);
        return;
    }
    send_response(STOP_MSG_CONFIG_RESULT, frame->request_id, payload,
                  (uint16_t)payload_len);
    publish_status();
}

static void handle_radio_send(const stop_frame_view_t *frame)
{
    if (ota_service_is_active()) {
        send_error(frame->request_id, STOP_ERROR_BUSY, false);
        return;
    }
    if (!s_radio_ready || frame->payload_len < 3U) {
        send_error(frame->request_id, STOP_ERROR_INVALID_STATE, false);
        return;
    }
    uint16_t data_len = stop_read_le16(frame->payload);
    if (data_len == 0U || data_len > E22_MAX_PAYLOAD_LEN ||
        frame->payload_len != data_len + 2U) {
        send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
        return;
    }
    device_config_t config;
    device_config_get(&config);
    esp_err_t err = e22_send(frame->payload + 2, data_len,
                             pdMS_TO_TICKS(config.radio_tx_timeout_ms));
    if (err != ESP_OK) {
        send_error(frame->request_id,
                   err == ESP_ERR_TIMEOUT ? STOP_ERROR_TIMEOUT : STOP_ERROR_INTERNAL,
                   false);
        return;
    }
    uint8_t response[2];
    stop_write_le16(response, data_len);
    send_response(STOP_MSG_RADIO_SEND_RESULT, frame->request_id,
                  response, sizeof(response));
}

static void send_ota_status(uint16_t request_id, bool response, bool bulk_ack,
                            stop_error_t operation_error)
{
    uint8_t payload[18];
    size_t payload_len = ota_service_encode_status(payload, sizeof(payload));
    uint8_t flags = response ? STOP_FLAG_RESPONSE : STOP_FLAG_EVENT;
    if (operation_error != STOP_ERROR_OK) {
        flags |= STOP_FLAG_ERROR;
        stop_write_le16(payload + 1, (uint16_t)operation_error);
    }
    if (bulk_ack) {
        flags |= STOP_FLAG_ACK;
    }
    esp_err_t err = send_frame(bulk_ack, STOP_MSG_OTA_STATUS, flags, request_id,
                               payload, (uint16_t)payload_len);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Unable to send OTA status: %s", esp_err_to_name(err));
    }
}

static void reboot_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

static void handle_control(const stop_frame_view_t *frame)
{
    if ((frame->flags & STOP_FLAG_REQUEST) == 0U || frame->request_id == 0U) {
        send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
        return;
    }
    switch (frame->message_type) {
    case STOP_MSG_DEVICE_INFO_GET: {
        if (frame->payload_len != 0U) {
            send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
            break;
        }
        uint8_t payload[64];
        uint16_t len = encode_device_info(payload, sizeof(payload));
        send_response(STOP_MSG_DEVICE_INFO_GET, frame->request_id, payload, len);
        break;
    }
    case STOP_MSG_DEVICE_STATUS_GET: {
        if (frame->payload_len != 0U) {
            send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
            break;
        }
        uint8_t payload[32];
        uint16_t len = encode_device_status(payload, sizeof(payload));
        send_response(STOP_MSG_DEVICE_STATUS_EVENT, frame->request_id, payload, len);
        break;
    }
    case STOP_MSG_CONFIG_GET:
        handle_config_get(frame);
        break;
    case STOP_MSG_CONFIG_SET:
        handle_config_set(frame);
        break;
    case STOP_MSG_RADIO_SEND:
        handle_radio_send(frame);
        break;
    case STOP_MSG_OTA_BEGIN: {
        uint16_t mtu = ble_transport_get_mtu();
        /* ATT value = MTU-3; STOP frame + OTA_DATA fixed fields consume 34 bytes. */
        uint16_t link_max_chunk = mtu > 37U ? (uint16_t)(mtu - 37U) : 0U;
        stop_error_t err = ota_service_begin(frame->payload, frame->payload_len,
                                             link_max_chunk);
        send_ota_status(frame->request_id, true, false, err);
        publish_status();
        break;
    }
    case STOP_MSG_OTA_QUERY: {
        ota_service_status_t status;
        ota_service_get_status(&status);
        if (frame->payload_len != 0U &&
            (frame->payload_len != 4U ||
             stop_read_le32(frame->payload) != status.transfer_id)) {
            send_error(frame->request_id, STOP_ERROR_INVALID_ARGUMENT, false);
        } else {
            send_ota_status(frame->request_id, true, false, STOP_ERROR_OK);
        }
        break;
    }
    case STOP_MSG_OTA_END: {
        stop_error_t err = ota_service_end(frame->payload, frame->payload_len);
        send_ota_status(frame->request_id, true, false, err);
        publish_status();
        if (err == STOP_ERROR_OK) {
            (void)xTaskCreate(reboot_task, "ota_reboot", 2048, NULL,
                              SERVICE_TASK_PRIORITY, NULL);
        }
        break;
    }
    case STOP_MSG_OTA_ABORT: {
        stop_error_t err = ota_service_abort(frame->payload, frame->payload_len);
        send_ota_status(frame->request_id, true, false, err);
        publish_status();
        break;
    }
    default:
        send_error(frame->request_id, STOP_ERROR_UNSUPPORTED_MESSAGE, false);
        break;
    }
}

static void transport_rx(ble_transport_channel_t channel, const uint8_t *data,
                         size_t len, void *context)
{
    (void)context;
    stop_frame_view_t frame;
    stop_error_t protocol_error;
    esp_err_t err = stop_frame_decode(data, len, &frame, &protocol_error);
    if (err != ESP_OK) {
        uint16_t request_id = len >= 8U ? stop_read_le16(data + 6) : 0U;
        send_error(request_id, protocol_error, channel == BLE_TRANSPORT_CHANNEL_BULK_DATA);
        return;
    }
    if (channel == BLE_TRANSPORT_CHANNEL_CONTROL) {
        handle_control(&frame);
        return;
    }
    if (frame.message_type != STOP_MSG_OTA_DATA ||
        (frame.flags & STOP_FLAG_REQUEST) == 0U) {
        send_error(frame.request_id, STOP_ERROR_UNSUPPORTED_MESSAGE, true);
        return;
    }
    stop_error_t ota_error = ota_service_write(frame.payload, frame.payload_len);
    send_ota_status(frame.request_id, true, true, ota_error);
}

static void status_task(void *arg)
{
    (void)arg;
    while (true) {
        publish_status();
        device_config_t config;
        device_config_get(&config);
        vTaskDelay(pdMS_TO_TICKS(config.status_period_ms));
    }
}

static void radio_rx_task(void *arg)
{
    (void)arg;
    while (true) {
        e22_rx_msg_t message;
        if (e22_receive(&message, portMAX_DELAY) != ESP_OK) {
            continue;
        }
        uint8_t payload[2U + E22_MAX_PAYLOAD_LEN];
        stop_write_le16(payload, message.len);
        memcpy(payload + 2, message.data, message.len);
        esp_err_t err = send_frame(false, STOP_MSG_RADIO_RX_EVENT, STOP_FLAG_EVENT,
                                   0, payload, (uint16_t)(message.len + 2U));
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE &&
            err != ESP_ERR_INVALID_SIZE) {
            ESP_LOGW(TAG, "Radio RX event failed: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t ble_device_service_start(bool radio_ready)
{
    s_radio_ready = radio_ready;
    esp_err_t err = device_config_init();
    if (err != ESP_OK) {
        return err;
    }
    ota_service_init();
    err = ble_transport_start(transport_rx, NULL);
    if (err != ESP_OK) {
        return err;
    }
    if (xTaskCreate(status_task, "ble_status", SERVICE_TASK_STACK, NULL,
                    SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (radio_ready &&
        xTaskCreate(radio_rx_task, "ble_radio_rx", SERVICE_TASK_STACK, NULL,
                    SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "BLE device services started; radio=%s",
             radio_ready ? "ready" : "unavailable");
    return ESP_OK;
}
