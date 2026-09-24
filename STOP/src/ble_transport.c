#include "ble_transport.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_TRANSPORT_RX_QUEUE_LEN       8U
#define BLE_TRANSPORT_WORKER_STACK       4096U
#define BLE_TRANSPORT_WORKER_PRIORITY    6U
#define BLE_TRANSPORT_DEFAULT_MTU        23U

static const char *TAG = "BLE_TRANSPORT";

/* UUID strings are represented in reverse byte order by BLE_UUID128_INIT. */
static const ble_uuid128_t s_service_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x00, 0x00, 0x61, 0x7f);
static const ble_uuid128_t s_control_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x01, 0x00, 0x61, 0x7f);
static const ble_uuid128_t s_bulk_data_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x02, 0x00, 0x61, 0x7f);
static const ble_uuid128_t s_event_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x03, 0x00, 0x61, 0x7f);
static const ble_uuid128_t s_status_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x04, 0x00, 0x61, 0x7f);
static const ble_uuid128_t s_bulk_ack_uuid =
    BLE_UUID128_INIT(0x10, 0x2d, 0xb8, 0xa4, 0xc6, 0x67, 0x62, 0x8a,
                     0x5e, 0x4b, 0x79, 0x9f, 0x05, 0x00, 0x61, 0x7f);

typedef struct {
    ble_transport_channel_t channel;
    uint16_t len;
    uint8_t data[BLE_TRANSPORT_MAX_FRAME_LEN];
} ble_transport_rx_item_t;

static QueueHandle_t s_rx_queue;
static SemaphoreHandle_t s_status_mutex;
static ble_transport_rx_handler_t s_rx_handler;
static void *s_rx_context;

static uint16_t s_control_handle;
static uint16_t s_bulk_data_handle;
static uint16_t s_event_handle;
static uint16_t s_status_handle;
static uint16_t s_bulk_ack_handle;

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_att_mtu = BLE_TRANSPORT_DEFAULT_MTU;
static int8_t s_last_rssi = 127;
static int64_t s_last_rssi_time_us;
static uint8_t s_own_addr_type;
static bool s_event_subscribed;
static bool s_status_subscribed;
static bool s_bulk_ack_subscribed;
static bool s_started;

static uint8_t s_status_value[BLE_TRANSPORT_MAX_STATUS_LEN];
static uint16_t s_status_len;

static int ble_transport_gap_event(struct ble_gap_event *event, void *arg);

static esp_err_t nimble_rc_to_esp_err(int rc)
{
    if (rc == 0) {
        return ESP_OK;
    }
    if (rc == BLE_HS_ENOMEM) {
        return ESP_ERR_NO_MEM;
    }
    if (rc == BLE_HS_ENOTCONN) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_FAIL;
}

static int queue_write(struct os_mbuf *om, ble_transport_channel_t channel)
{
    uint16_t len = OS_MBUF_PKTLEN(om);
    if (len == 0 || len > BLE_TRANSPORT_MAX_FRAME_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    ble_transport_rx_item_t item = {
        .channel = channel,
        .len = len,
    };
    int rc = ble_hs_mbuf_to_flat(om, item.data, sizeof(item.data), NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "RX queue full; dropping channel %u frame",
                 (unsigned)channel);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (attr_handle == s_control_handle) {
            return queue_write(ctxt->om, BLE_TRANSPORT_CHANNEL_CONTROL);
        }
        if (attr_handle == s_bulk_data_handle) {
            return queue_write(ctxt->om, BLE_TRANSPORT_CHANNEL_BULK_DATA);
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR &&
        attr_handle == s_status_handle) {
        uint8_t snapshot[BLE_TRANSPORT_MAX_STATUS_LEN];
        uint16_t snapshot_len = 0;

        if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        snapshot_len = s_status_len;
        memcpy(snapshot, s_status_value, snapshot_len);
        xSemaphoreGive(s_status_mutex);

        return os_mbuf_append(ctxt->om, snapshot, snapshot_len) == 0
                   ? 0
                   : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_control_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_control_handle,
            },
            {
                .uuid = &s_bulk_data_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_bulk_data_handle,
            },
            {
                .uuid = &s_event_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_event_handle,
            },
            {
                .uuid = &s_status_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_handle,
            },
            {
                .uuid = &s_bulk_ack_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_bulk_ack_handle,
            },
            {0},
        },
    },
    {0},
};

static int gatt_server_init(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_gatt_services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_gatt_services);
    }
    return rc;
}

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)BLE_TRANSPORT_DEVICE_NAME;
    fields.name_len = strlen(BLE_TRANSPORT_DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.uuids128 = (ble_uuid128_t *)&s_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data: rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &params, ble_transport_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising: rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as %s", BLE_TRANSPORT_DEVICE_NAME);
    }
}

static int ble_transport_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_last_rssi = 127;
            s_last_rssi_time_us = 0;
            uint16_t negotiated_mtu = ble_att_mtu(s_conn_handle);
            s_att_mtu = negotiated_mtu >= BLE_TRANSPORT_DEFAULT_MTU
                            ? negotiated_mtu
                            : BLE_TRANSPORT_DEFAULT_MTU;
            ESP_LOGI(TAG, "Maintenance client connected; handle=%u mtu=%u",
                     s_conn_handle, s_att_mtu);
        } else {
            ESP_LOGW(TAG, "Connection failed: status=%d",
                     event->connect.status);
            advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Maintenance client disconnected; reason=%d",
                 event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_last_rssi = 127;
        s_last_rssi_time_us = 0;
        s_att_mtu = BLE_TRANSPORT_DEFAULT_MTU;
        s_event_subscribed = false;
        s_status_subscribed = false;
        s_bulk_ack_subscribed = false;
        advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_event_handle) {
            s_event_subscribed = event->subscribe.cur_notify != 0;
        } else if (event->subscribe.attr_handle == s_status_handle) {
            s_status_subscribed = event->subscribe.cur_notify != 0;
        } else if (event->subscribe.attr_handle == s_bulk_ack_handle) {
            s_bulk_ack_subscribed = event->subscribe.cur_notify != 0;
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        s_att_mtu = event->mtu.value;
        ESP_LOGI(TAG, "ATT MTU updated to %u", s_att_mtu);
        return 0;

    default:
        return 0;
    }
}

static void host_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset; reason=%d", reason);
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

static void host_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "No usable BLE identity address: rc=%d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer BLE address type: rc=%d", rc);
        return;
    }
    advertise();
}

static void host_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void rx_worker(void *arg)
{
    (void)arg;
    ble_transport_rx_item_t item;

    while (true) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE &&
            s_rx_handler != NULL) {
            s_rx_handler(item.channel, item.data, item.len, s_rx_context);
        }
    }
}

static esp_err_t send_notification(uint16_t attr_handle, bool subscribed,
                                   const void *data, size_t len)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !subscribed) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t negotiated_mtu = ble_att_mtu(s_conn_handle);
    if (negotiated_mtu >= BLE_TRANSPORT_DEFAULT_MTU) {
        s_att_mtu = negotiated_mtu;
    }
    if (len > BLE_TRANSPORT_MAX_FRAME_LEN ||
        len > (size_t)(s_att_mtu - 3U)) {
        return ESP_ERR_INVALID_SIZE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return nimble_rc_to_esp_err(
        ble_gatts_notify_custom(s_conn_handle, attr_handle, om));
}

esp_err_t ble_transport_start(ble_transport_rx_handler_t rx_handler,
                              void *context)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    s_rx_queue = xQueueCreate(BLE_TRANSPORT_RX_QUEUE_LEN,
                              sizeof(ble_transport_rx_item_t));
    s_status_mutex = xSemaphoreCreateMutex();
    if (s_rx_queue == NULL || s_status_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_rx_handler = rx_handler;
    s_rx_context = context;

    if (xTaskCreate(rx_worker, "ble_rx", BLE_TRANSPORT_WORKER_STACK, NULL,
                    BLE_TRANSPORT_WORKER_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = host_reset;
    ble_hs_cfg.sync_cb = host_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

    int rc = gatt_server_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT service initialization failed: rc=%d", rc);
        return nimble_rc_to_esp_err(rc);
    }

    rc = ble_svc_gap_device_name_set(BLE_TRANSPORT_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set BLE device name: rc=%d", rc);
        return nimble_rc_to_esp_err(rc);
    }

    s_started = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

esp_err_t ble_transport_send_event(const void *data, size_t len)
{
    return send_notification(s_event_handle, s_event_subscribed, data, len);
}

esp_err_t ble_transport_publish_status(const void *data, size_t len)
{
    if (data == NULL || len == 0 || len > sizeof(s_status_value)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    memcpy(s_status_value, data, len);
    s_status_len = len;
    xSemaphoreGive(s_status_mutex);

    if (!s_status_subscribed) {
        return ESP_OK;
    }
    return send_notification(s_status_handle, true, data, len);
}

esp_err_t ble_transport_send_bulk_ack(const void *data, size_t len)
{
    return send_notification(s_bulk_ack_handle, s_bulk_ack_subscribed,
                             data, len);
}

bool ble_transport_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

uint16_t ble_transport_get_mtu(void)
{
    return s_att_mtu;
}

esp_err_t ble_transport_get_rssi(int8_t *rssi)
{
    if (rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t conn_handle = s_conn_handle;
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    int64_t now = esp_timer_get_time();
    if (s_last_rssi != 127 && now - s_last_rssi_time_us < 2000000LL) {
        *rssi = s_last_rssi;
        return ESP_OK;
    }
    int8_t value = 127;
    int rc = ble_gap_conn_rssi(conn_handle, &value);
    if (rc != 0 || value == 127) {
        return nimble_rc_to_esp_err(rc != 0 ? rc : BLE_HS_EUNKNOWN);
    }
    s_last_rssi = value;
    s_last_rssi_time_us = now;
    *rssi = value;
    return ESP_OK;
}
