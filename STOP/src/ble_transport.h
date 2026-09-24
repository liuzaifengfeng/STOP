#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_TRANSPORT_DEVICE_NAME       "STOP-C6"
#define BLE_TRANSPORT_MAX_FRAME_LEN     512U
#define BLE_TRANSPORT_MAX_STATUS_LEN    256U

#define BLE_TRANSPORT_SERVICE_UUID      "7f610000-9f79-4b5e-8a62-67c6a4b82d10"
#define BLE_TRANSPORT_CONTROL_UUID      "7f610001-9f79-4b5e-8a62-67c6a4b82d10"
#define BLE_TRANSPORT_BULK_DATA_UUID    "7f610002-9f79-4b5e-8a62-67c6a4b82d10"
#define BLE_TRANSPORT_EVENT_UUID        "7f610003-9f79-4b5e-8a62-67c6a4b82d10"
#define BLE_TRANSPORT_STATUS_UUID       "7f610004-9f79-4b5e-8a62-67c6a4b82d10"
#define BLE_TRANSPORT_BULK_ACK_UUID     "7f610005-9f79-4b5e-8a62-67c6a4b82d10"

typedef enum {
    BLE_TRANSPORT_CHANNEL_CONTROL = 0,
    BLE_TRANSPORT_CHANNEL_BULK_DATA,
} ble_transport_channel_t;

/**
 * Called from the BLE transport worker task, never from the NimBLE host task.
 * The data buffer is owned by the transport and is valid only during the call.
 */
typedef void (*ble_transport_rx_handler_t)(ble_transport_channel_t channel,
                                           const uint8_t *data,
                                           size_t len,
                                           void *context);

/** Start the NimBLE peripheral, GATT service, worker task and advertising. */
esp_err_t ble_transport_start(ble_transport_rx_handler_t rx_handler,
                              void *context);

/** Publish an asynchronous frame through the EVENT notification channel. */
esp_err_t ble_transport_send_event(const void *data, size_t len);

/** Replace the readable status value and notify a subscribed client. */
esp_err_t ble_transport_publish_status(const void *data, size_t len);

/** Publish bulk-transfer flow-control information through BULK_ACK. */
esp_err_t ble_transport_send_bulk_ack(const void *data, size_t len);

/** Return true while one maintenance client is connected. */
bool ble_transport_is_connected(void);

/** Return the most recently negotiated ATT MTU, or 23 before negotiation. */
uint16_t ble_transport_get_mtu(void);

/** Read the current connection RSSI in dBm. */
esp_err_t ble_transport_get_rssi(int8_t *rssi);

#ifdef __cplusplus
}
#endif

#endif /* BLE_TRANSPORT_H */
