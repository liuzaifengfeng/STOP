#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "stop_protocol.h"

#define STOP_PRODUCT_ID_BUTTON_BOX 0x0001U
#define STOP_HARDWARE_REVISION     0x0001U
#define OTA_SERVICE_MAX_CHUNK      478U

typedef enum {
    OTA_SERVICE_IDLE = 0,
    OTA_SERVICE_PREPARING = 1,
    OTA_SERVICE_RECEIVING = 2,
    OTA_SERVICE_VERIFYING = 3,
    OTA_SERVICE_READY_TO_REBOOT = 4,
    OTA_SERVICE_FAILED = 5,
    OTA_SERVICE_ABORTED = 6,
} ota_service_state_t;

typedef struct {
    ota_service_state_t state;
    stop_error_t last_error;
    uint32_t transfer_id;
    uint32_t expected_offset;
    uint32_t image_size;
    uint16_t accepted_chunk_size;
    bool reboot_pending;
} ota_service_status_t;

void ota_service_init(void);
stop_error_t ota_service_begin(const uint8_t *payload, size_t payload_len,
                               uint16_t link_max_chunk);
stop_error_t ota_service_write(const uint8_t *payload, size_t payload_len);
stop_error_t ota_service_end(const uint8_t *payload, size_t payload_len);
stop_error_t ota_service_abort(const uint8_t *payload, size_t payload_len);
void ota_service_get_status(ota_service_status_t *status);
size_t ota_service_encode_status(uint8_t *output, size_t capacity);
bool ota_service_is_active(void);

#endif
