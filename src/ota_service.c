#include "ota_service.h"

#include <string.h>

#include "adc_monitor.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "sha256_sw.h"

#define OTA_MIN_BATTERY_SOC 20
#define OTA_RESULT_NAMESPACE "ota_result"
#define OTA_RESULT_KEY       "record"
#define OTA_RESULT_MAGIC     0x5241544fUL
#define OTA_RESULT_FORMAT    1U
#define OTA_RESULT_RECORD_LEN 56U

static const char *TAG = "OTA_SERVICE";

typedef struct {
    ota_service_status_t status;
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    uint8_t expected_sha256[32];
    char expected_version[32];
    sha256_sw_context_t sha256;
    bool handle_open;
    bool hash_started;
} ota_context_t;

static ota_context_t s_ota;
static bool s_hash_ready;
static ota_service_result_t s_result;
static uint32_t s_result_target_address;

static void encode_result_record(uint8_t record[OTA_RESULT_RECORD_LEN])
{
    memset(record, 0, OTA_RESULT_RECORD_LEN);
    stop_write_le32(record, OTA_RESULT_MAGIC);
    record[4] = OTA_RESULT_FORMAT;
    record[5] = (uint8_t)s_result.state;
    stop_write_le16(record + 6, (uint16_t)s_result.last_error);
    stop_write_le32(record + 8, s_result.transfer_id);
    stop_write_le32(record + 12, s_result.image_size);
    stop_write_le32(record + 16, s_result_target_address);
    memcpy(record + 20, s_result.image_sha256, sizeof(s_result.image_sha256));
    stop_write_le32(record + 52, stop_crc32_iso_hdlc(record, 52));
}

static esp_err_t save_result(void)
{
    uint8_t record[OTA_RESULT_RECORD_LEN];
    encode_result_record(record);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(OTA_RESULT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, OTA_RESULT_KEY, record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void load_result(void)
{
    memset(&s_result, 0, sizeof(s_result));
    s_result_target_address = 0U;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(OTA_RESULT_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Unable to open OTA result record: %s", esp_err_to_name(err));
        return;
    }
    uint8_t record[OTA_RESULT_RECORD_LEN];
    size_t length = sizeof(record);
    err = nvs_get_blob(handle, OTA_RESULT_KEY, record, &length);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }
    if (err != ESP_OK || length != sizeof(record) ||
        stop_read_le32(record) != OTA_RESULT_MAGIC ||
        record[4] != OTA_RESULT_FORMAT ||
        record[5] > OTA_RESULT_FAILED ||
        stop_read_le32(record + 52) != stop_crc32_iso_hdlc(record, 52)) {
        ESP_LOGW(TAG, "Ignoring invalid OTA result record");
        return;
    }
    s_result.state = (ota_result_state_t)record[5];
    s_result.last_error = (stop_error_t)stop_read_le16(record + 6);
    s_result.transfer_id = stop_read_le32(record + 8);
    s_result.image_size = stop_read_le32(record + 12);
    s_result_target_address = stop_read_le32(record + 16);
    memcpy(s_result.image_sha256, record + 20, sizeof(s_result.image_sha256));
}

static void close_handle(bool finish)
{
    if (s_ota.handle_open) {
        if (!finish) {
            (void)esp_ota_abort(s_ota.handle);
        }
        s_ota.handle_open = false;
    }
    if (s_ota.hash_started) {
        memset(&s_ota.sha256, 0, sizeof(s_ota.sha256));
        s_ota.hash_started = false;
    }
}

static stop_error_t fail(stop_error_t error)
{
    close_handle(false);
    s_ota.status.state = OTA_SERVICE_FAILED;
    s_ota.status.last_error = error;
    return error;
}

void ota_service_init(void)
{
    memset(&s_ota, 0, sizeof(s_ota));
    s_ota.status.state = OTA_SERVICE_IDLE;
    load_result();
    if (s_result.state == OTA_RESULT_AWAITING_CONFIRMATION) {
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (running == NULL || running->address != s_result_target_address) {
            s_result.state = OTA_RESULT_FAILED;
            s_result.last_error = STOP_ERROR_IMAGE_REJECTED;
            esp_err_t result_err = save_result();
            ESP_LOGW(TAG, "OTA candidate did not remain active; rollback detected%s",
                     result_err == ESP_OK ? "" : " (result persistence failed)");
        } else {
            ESP_LOGI(TAG, "OTA candidate is running and awaits service confirmation");
        }
    }
    s_hash_ready = sha256_sw_self_test();
    if (!s_hash_ready) {
        ESP_LOGE(TAG, "Software SHA-256 self-test failed; OTA disabled");
    } else {
        ESP_LOGI(TAG, "Software SHA-256 self-test passed");
    }
}

stop_error_t ota_service_begin(const uint8_t *payload, size_t payload_len,
                               uint16_t link_max_chunk)
{
    /* Fixed fields + version_len + sha256 + signature_len. */
    if (!s_hash_ready) {
        return STOP_ERROR_INTERNAL;
    }
    if (payload == NULL || payload_len < 49U) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    if (s_ota.handle_open || s_ota.status.reboot_pending) {
        return STOP_ERROR_BUSY;
    }

    uint32_t transfer_id = stop_read_le32(payload);
    uint32_t image_size = stop_read_le32(payload + 4);
    uint16_t requested_chunk = stop_read_le16(payload + 8);
    uint16_t product_id = stop_read_le16(payload + 10);
    uint16_t hardware_rev = stop_read_le16(payload + 12);
    uint8_t version_len = payload[14];
    size_t sha_offset = 15U + version_len;
    if (transfer_id == 0U || image_size == 0U || requested_chunk == 0U ||
        link_max_chunk == 0U ||
        version_len == 0U || version_len > 31U ||
        sha_offset + 34U > payload_len) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    uint16_t signature_len = stop_read_le16(payload + sha_offset + 32U);
    if (sha_offset + 34U + signature_len != payload_len) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    if (signature_len != 0U) {
        /* Application-level public-key verification is not provisioned yet. */
        return STOP_ERROR_IMAGE_REJECTED;
    }
    if (product_id != STOP_PRODUCT_ID_BUTTON_BOX ||
        hardware_rev != STOP_HARDWARE_REVISION) {
        return STOP_ERROR_IMAGE_REJECTED;
    }

    BatteryInfo battery;
    if (get_battery_info(&battery) && battery.soc < OTA_MIN_BATTERY_SOC) {
        return STOP_ERROR_SAFETY_LOCKOUT;
    }

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL || image_size > partition->size) {
        return STOP_ERROR_IMAGE_REJECTED;
    }

    close_handle(false);
    memset(&s_ota, 0, sizeof(s_ota));
    s_ota.status.state = OTA_SERVICE_PREPARING;
    s_ota.status.transfer_id = transfer_id;
    s_ota.status.image_size = image_size;
    uint16_t accepted_chunk = requested_chunk < OTA_SERVICE_MAX_CHUNK
                                  ? requested_chunk
                                  : OTA_SERVICE_MAX_CHUNK;
    s_ota.status.accepted_chunk_size =
        accepted_chunk < link_max_chunk ? accepted_chunk : link_max_chunk;
    s_ota.partition = partition;
    memcpy(s_ota.expected_sha256, payload + sha_offset, sizeof(s_ota.expected_sha256));
    memcpy(s_ota.expected_version, payload + 15U, version_len);
    s_ota.expected_version[version_len] = '\0';

    esp_err_t err = esp_ota_begin(partition, image_size, &s_ota.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return fail(STOP_ERROR_INTERNAL);
    }
    s_ota.handle_open = true;
    sha256_sw_init(&s_ota.sha256);
    s_ota.hash_started = true;
    s_ota.status.state = OTA_SERVICE_RECEIVING;
    s_ota.status.last_error = STOP_ERROR_OK;
    ESP_LOGI(TAG, "OTA started: id=%lu size=%lu partition=%s",
             (unsigned long)transfer_id, (unsigned long)image_size, partition->label);
    return STOP_ERROR_OK;
}

stop_error_t ota_service_write(const uint8_t *payload, size_t payload_len)
{
    if (payload == NULL || payload_len < 14U || !s_ota.handle_open ||
        s_ota.status.state != OTA_SERVICE_RECEIVING) {
        return STOP_ERROR_INVALID_STATE;
    }
    uint32_t transfer_id = stop_read_le32(payload);
    uint32_t offset = stop_read_le32(payload + 4);
    uint16_t data_len = stop_read_le16(payload + 8);
    if (transfer_id != s_ota.status.transfer_id || data_len == 0U ||
        data_len > s_ota.status.accepted_chunk_size ||
        payload_len != 10U + data_len + 4U) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    if (offset != s_ota.status.expected_offset) {
        s_ota.status.last_error = STOP_ERROR_OFFSET_MISMATCH;
        return STOP_ERROR_OFFSET_MISMATCH;
    }
    if ((uint64_t)offset + data_len > s_ota.status.image_size ||
        stop_read_le32(payload + 10U + data_len) !=
            stop_crc32_iso_hdlc(payload + 10U, data_len)) {
        s_ota.status.last_error = STOP_ERROR_CRC;
        return STOP_ERROR_CRC;
    }

    esp_err_t err = esp_ota_write(s_ota.handle, payload + 10U, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA write failed at %lu: %s", (unsigned long)offset,
                 esp_err_to_name(err));
        return fail(STOP_ERROR_INTERNAL);
    }
    sha256_sw_update(&s_ota.sha256, payload + 10U, data_len);
    s_ota.status.expected_offset += data_len;
    s_ota.status.last_error = STOP_ERROR_OK;
    return STOP_ERROR_OK;
}

stop_error_t ota_service_end(const uint8_t *payload, size_t payload_len)
{
    if (payload == NULL || payload_len != 4U || !s_ota.handle_open ||
        s_ota.status.state != OTA_SERVICE_RECEIVING) {
        return STOP_ERROR_INVALID_STATE;
    }
    if (stop_read_le32(payload) != s_ota.status.transfer_id ||
        s_ota.status.expected_offset != s_ota.status.image_size) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }

    s_ota.status.state = OTA_SERVICE_VERIFYING;
    uint8_t actual_sha256[32];
    sha256_sw_finish(&s_ota.sha256, actual_sha256);
    s_ota.hash_started = false;
    if (memcmp(actual_sha256, s_ota.expected_sha256, sizeof(actual_sha256)) != 0) {
        ESP_LOGE(TAG, "OTA SHA-256 mismatch");
        return fail(STOP_ERROR_IMAGE_REJECTED);
    }

    esp_err_t err = esp_ota_end(s_ota.handle);
    s_ota.handle_open = false;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA image validation failed: %s", esp_err_to_name(err));
        s_ota.status.state = OTA_SERVICE_FAILED;
        s_ota.status.last_error = STOP_ERROR_IMAGE_REJECTED;
        return STOP_ERROR_IMAGE_REJECTED;
    }
    esp_app_desc_t candidate_desc;
    const esp_app_desc_t *running_desc = esp_app_get_description();
    err = esp_ota_get_partition_description(s_ota.partition, &candidate_desc);
    if (err != ESP_OK ||
        strncmp(candidate_desc.project_name, running_desc->project_name,
                sizeof(candidate_desc.project_name)) != 0 ||
        strncmp(candidate_desc.version, s_ota.expected_version,
                sizeof(candidate_desc.version)) != 0) {
        ESP_LOGE(TAG, "OTA project or version identity mismatch");
        s_ota.status.state = OTA_SERVICE_FAILED;
        s_ota.status.last_error = STOP_ERROR_IMAGE_REJECTED;
        return STOP_ERROR_IMAGE_REJECTED;
    }

    memset(&s_result, 0, sizeof(s_result));
    s_result.state = OTA_RESULT_AWAITING_CONFIRMATION;
    s_result.last_error = STOP_ERROR_OK;
    s_result.transfer_id = s_ota.status.transfer_id;
    s_result.image_size = s_ota.status.image_size;
    memcpy(s_result.image_sha256, s_ota.expected_sha256,
           sizeof(s_result.image_sha256));
    s_result_target_address = s_ota.partition->address;
    err = save_result();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to persist OTA handoff record: %s",
                 esp_err_to_name(err));
        s_ota.status.state = OTA_SERVICE_FAILED;
        s_ota.status.last_error = STOP_ERROR_INTERNAL;
        return STOP_ERROR_INTERNAL;
    }
    err = esp_ota_set_boot_partition(s_ota.partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select OTA boot partition: %s", esp_err_to_name(err));
        s_result.state = OTA_RESULT_FAILED;
        s_result.last_error = STOP_ERROR_INTERNAL;
        (void)save_result();
        s_ota.status.state = OTA_SERVICE_FAILED;
        s_ota.status.last_error = STOP_ERROR_INTERNAL;
        return STOP_ERROR_INTERNAL;
    }

    s_ota.status.state = OTA_SERVICE_READY_TO_REBOOT;
    s_ota.status.last_error = STOP_ERROR_OK;
    s_ota.status.reboot_pending = true;
    ESP_LOGI(TAG, "OTA verified; reboot scheduled");
    return STOP_ERROR_OK;
}

stop_error_t ota_service_abort(const uint8_t *payload, size_t payload_len)
{
    if (payload == NULL || payload_len != 4U ||
        stop_read_le32(payload) != s_ota.status.transfer_id) {
        return STOP_ERROR_INVALID_ARGUMENT;
    }
    close_handle(false);
    s_ota.status.state = OTA_SERVICE_ABORTED;
    s_ota.status.last_error = STOP_ERROR_OK;
    return STOP_ERROR_OK;
}

void ota_service_get_status(ota_service_status_t *status)
{
    if (status != NULL) {
        *status = s_ota.status;
    }
}

size_t ota_service_encode_status(uint8_t *output, size_t capacity)
{
    if (output == NULL || capacity < 18U) {
        return 0U;
    }
    output[0] = (uint8_t)s_ota.status.state;
    stop_write_le16(output + 1, (uint16_t)s_ota.status.last_error);
    stop_write_le32(output + 3, s_ota.status.transfer_id);
    stop_write_le32(output + 7, s_ota.status.expected_offset);
    stop_write_le32(output + 11, s_ota.status.image_size);
    stop_write_le16(output + 15, s_ota.status.accepted_chunk_size);
    output[17] = s_ota.status.reboot_pending ? 1U : 0U;
    return 18U;
}

size_t ota_service_encode_result(uint8_t *output, size_t capacity)
{
    if (output == NULL || capacity < 43U) {
        return 0U;
    }
    output[0] = (uint8_t)s_result.state;
    stop_write_le16(output + 1, (uint16_t)s_result.last_error);
    stop_write_le32(output + 3, s_result.transfer_id);
    stop_write_le32(output + 7, s_result.image_size);
    memcpy(output + 11, s_result.image_sha256, sizeof(s_result.image_sha256));
    return 43U;
}

void ota_service_mark_running_image_confirmed(void)
{
    if (s_result.state != OTA_RESULT_AWAITING_CONFIRMATION) {
        return;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL || running->address != s_result_target_address) {
        return;
    }
    s_result.state = OTA_RESULT_CONFIRMED;
    s_result.last_error = STOP_ERROR_OK;
    esp_err_t err = save_result();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA result confirmed and persisted: id=%lu",
                 (unsigned long)s_result.transfer_id);
    } else {
        ESP_LOGE(TAG, "Unable to persist confirmed OTA result: %s",
                 esp_err_to_name(err));
    }
}

bool ota_service_is_active(void)
{
    return s_ota.handle_open || s_ota.status.reboot_pending;
}
