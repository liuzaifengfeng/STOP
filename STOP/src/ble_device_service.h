#ifndef BLE_DEVICE_SERVICE_H
#define BLE_DEVICE_SERVICE_H

#include <stdbool.h>

#include "esp_err.h"

/** Start the STOP frame dispatcher, configuration, OTA and status services. */
esp_err_t ble_device_service_start(bool radio_ready);

#endif
