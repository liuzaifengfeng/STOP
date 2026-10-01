#include "radio_transport.h"

#include "device_config.h"
#include <string.h>
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "ESPNOW";
static const uint8_t s_broadcast[6] = {255, 255, 255, 255, 255, 255};
typedef struct {
    radio_rx_msg_t msg;
    TickType_t received_at;
} now_rx_t;
static QueueHandle_t s_rx_queue;
static SemaphoreHandle_t s_tx_lock, s_tx_done;
static volatile esp_now_send_status_t s_tx_status;
static bool s_ready, s_tx_pending;

/* Wi-Fi 高优先级回调只复制数据。认证、配对、NVS 和回执都在业务任务执行。 */
static void receive_callback(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!info || !data || len <= 0 || len > RADIO_MAX_PAYLOAD_LEN) return;
    now_rx_t packet = {.msg.len = (uint8_t)len, .received_at = xTaskGetTickCount()};
    memcpy(packet.msg.data, data, (size_t)len);
    /* 队列满时丢包，不阻塞 Wi-Fi；业务层仍按心跳截止时间处理失联。 */
    (void)xQueueSend(s_rx_queue, &packet, 0);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void send_callback(const esp_now_send_info_t *info, esp_now_send_status_t status)
#else
static void send_callback(const uint8_t *info, esp_now_send_status_t status)
#endif
{
    (void)info;
    s_tx_status = status;
    xSemaphoreGive(s_tx_done);
}

static esp_err_t espnow_init(uint8_t channel)
{
    if (s_ready) return ESP_OK;
    bool wifi_initialized = false, wifi_started = false, now_initialized = false;
    s_rx_queue = xQueueCreate(16, sizeof(now_rx_t));
    s_tx_lock = xSemaphoreCreateMutex();
    s_tx_done = xSemaphoreCreateBinary();
    esp_err_t err = ESP_ERR_NO_MEM;
    if (!s_rx_queue || !s_tx_lock || !s_tx_done) goto fail;

    err = esp_netif_init();
    if (err != ESP_OK) goto fail;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi);
    if (err != ESP_OK) goto fail;
    wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;
    /* 不连接路由器，不获取 IP；STA 仅用于 ESP-NOW。 */
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    wifi_started = true;
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) goto fail;
    err = esp_now_init();
    if (err != ESP_OK) goto fail;
    now_initialized = true;
    err = esp_now_register_recv_cb(receive_callback);
    if (err != ESP_OK) goto fail;
    err = esp_now_register_send_cb(send_callback);
    if (err != ESP_OK) goto fail;
    esp_now_peer_info_t peer = {
        .channel = channel, .ifidx = WIFI_IF_STA, .encrypt = false,
    };
    memcpy(peer.peer_addr, s_broadcast, sizeof(s_broadcast));
    err = esp_now_add_peer(&peer);
    if (err != ESP_OK) goto fail;
    s_ready = true;
    ESP_LOGI(TAG, "Ready: channel=%u, broadcast transport, application pairing/authentication",
             (unsigned)channel);
    return ESP_OK;

fail:
    /* 停止回调来源后才释放队列；初始化失败交给主程序保持故障状态。 */
    if (now_initialized) esp_now_deinit();
    if (wifi_started) esp_wifi_stop();
    if (wifi_initialized) esp_wifi_deinit();
    if (s_rx_queue) vQueueDelete(s_rx_queue);
    if (s_tx_done) vSemaphoreDelete(s_tx_done);
    if (s_tx_lock) vSemaphoreDelete(s_tx_lock);
    s_rx_queue = NULL;
    s_tx_done = s_tx_lock = NULL;
    return err;
}

static TickType_t remaining(TickType_t start, TickType_t timeout)
{
    if (timeout == portMAX_DELAY) return portMAX_DELAY;
    TickType_t elapsed = xTaskGetTickCount() - start;
    return elapsed < timeout ? timeout - elapsed : 0;
}

static esp_err_t espnow_send(const uint8_t *data, size_t len, TickType_t timeout)
{
    if (!data || len == 0 || len > RADIO_MAX_PAYLOAD_LEN) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    TickType_t start = xTaskGetTickCount();
    if (xSemaphoreTake(s_tx_lock, timeout) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = ESP_ERR_TIMEOUT;
    /* 上次若超时，先消耗它的迟到回调，不能误认成本次发送结果。 */
    if (s_tx_pending) {
        if (xSemaphoreTake(s_tx_done, remaining(start, timeout)) != pdTRUE) goto done;
        s_tx_pending = false;
    }
    if (timeout && !remaining(start, timeout)) goto done;
    s_tx_pending = true;
    err = esp_now_send(s_broadcast, data, len);
    if (err != ESP_OK) {
        s_tx_pending = false;
        goto done;
    }
    if (xSemaphoreTake(s_tx_done, remaining(start, timeout)) != pdTRUE) {
        err = ESP_ERR_TIMEOUT;
        goto done;
    }
    s_tx_pending = false;
    /* 这里只确认无线发送完成；停机成功仍必须收到业务层 STOP_ACK。 */
    err = s_tx_status == ESP_NOW_SEND_SUCCESS ? ESP_OK : ESP_FAIL;
done:
    xSemaphoreGive(s_tx_lock);
    return err;
}

/* 双通道的 E22 发送独立执行，AUX 故障不得拖慢 ESP-NOW 心跳。
 * 单槽只保留最新一帧，避免旧的正常心跳排队后延迟维持输出许可。
 */
typedef struct {
    radio_rx_msg_t msg;
    TickType_t queued_at;
    TickType_t timeout;
} e22_job_t;
static QueueHandle_t s_e22_tx_queue;
static bool s_e22_ready;
static uint32_t s_link;

static void e22_tx_task(void *arg)
{
    (void)arg;
    e22_job_t job;
    while (true) {
        if (xQueueReceive(s_e22_tx_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        if (xTaskGetTickCount() - job.queued_at >= pdMS_TO_TICKS(200)) continue;
        esp_err_t err = e22_send(job.msg.data, job.msg.len, job.timeout);
        if (err != ESP_OK) ESP_LOGW(TAG, "Dual E22 TX: %s", esp_err_to_name(err));
    }
}

esp_err_t radio_transport_init(void)
{
    esp_err_t err = device_config_init();
    if (err != ESP_OK) return err;
    device_config_t config;
    device_config_get(&config);
    s_link = config.radio_link;
    esp_err_t e22_err = ESP_ERR_INVALID_STATE, now_err = ESP_ERR_INVALID_STATE;
    if (s_link != RADIO_LINK_ESPNOW) {
        e22_err = e22_init();
        s_e22_ready = e22_err == ESP_OK;
    } else {
        /* ESP-NOW 单通道无需 E22 自检；让外置模块保持休眠。 */
        gpio_set_level(E22_PIN_M0, 1);
        gpio_set_direction(E22_PIN_M0, GPIO_MODE_OUTPUT);
        gpio_set_level(E22_PIN_M1, 1);
        gpio_set_direction(E22_PIN_M1, GPIO_MODE_OUTPUT);
    }
    if (s_link != RADIO_LINK_E22) now_err = espnow_init((uint8_t)config.espnow_channel);
    if (s_link == RADIO_LINK_DUAL && s_e22_ready) {
        s_e22_tx_queue = xQueueCreate(1, sizeof(e22_job_t));
        if (!s_e22_tx_queue ||
            xTaskCreate(e22_tx_task, "dual_e22_tx", 3072, NULL, 6, NULL) != pdPASS) {
            if (s_e22_tx_queue) vQueueDelete(s_e22_tx_queue);
            s_e22_tx_queue = NULL;
            s_e22_ready = false;
            e22_err = ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "Mode=%s, E22=%s, ESP-NOW=%s (channel %lu)",
             radio_transport_name(), esp_err_to_name(e22_err), esp_err_to_name(now_err),
             (unsigned long)config.espnow_channel);
    return s_e22_ready || s_ready ? ESP_OK : (s_link == RADIO_LINK_E22 ? e22_err : now_err);
}

esp_err_t radio_transport_send(const uint8_t *data, size_t len, TickType_t timeout)
{
    if (!data || !len || len > RADIO_MAX_PAYLOAD_LEN) return ESP_ERR_INVALID_ARG;
    if (s_link == RADIO_LINK_E22) return e22_send(data, len, timeout);
    if (s_link == RADIO_LINK_ESPNOW) return espnow_send(data, len, timeout);
    bool queued = false;
    if (s_e22_ready && s_e22_tx_queue) {
        e22_job_t job = {.msg.len = (uint8_t)len, .queued_at = xTaskGetTickCount(),
                         .timeout = timeout};
        memcpy(job.msg.data, data, len);
        queued = xQueueOverwrite(s_e22_tx_queue, &job) == pdTRUE;
    }
    /* E22 只表示已交给后台发送；任一路发送成功都不等价于对端已执行。 */
    esp_err_t err = espnow_send(data, len, timeout);
    return err == ESP_OK || queued ? ESP_OK : err;
}

esp_err_t radio_transport_receive(radio_rx_msg_t *msg, TickType_t timeout)
{
    if (!msg) return ESP_ERR_INVALID_ARG;
    if (!s_e22_ready && !s_ready) return ESP_ERR_INVALID_STATE;
    if (s_link == RADIO_LINK_E22) return e22_receive(msg, timeout);
    TickType_t start = xTaskGetTickCount();
    do {
        /* 轮流优先取包，避免任一路的持续流量饿死另一条链路。 */
        static bool e22_first;
        e22_first = !e22_first;
        if (e22_first && s_e22_ready && e22_receive(msg, 0) == ESP_OK) return ESP_OK;
        now_rx_t packet;
        if (s_ready && xQueueReceive(s_rx_queue, &packet, 0) == pdTRUE &&
            xTaskGetTickCount() - packet.received_at < pdMS_TO_TICKS(200)) {
            *msg = packet.msg;
            return ESP_OK;
        }
        if (!e22_first && s_e22_ready && e22_receive(msg, 0) == ESP_OK) return ESP_OK;
        if (!remaining(start, timeout)) break;
        vTaskDelay(1);
    } while (true);
    return ESP_ERR_TIMEOUT;
}

const char *radio_transport_name(void)
{
    return s_link == RADIO_LINK_DUAL ? "E22 + ESP-NOW" :
           s_link == RADIO_LINK_ESPNOW ? "ESP-NOW" : "E22";
}
uint8_t radio_transport_mode(void)
{
    return s_link == RADIO_LINK_DUAL ? RADIO_MODE_DUAL :
           s_link == RADIO_LINK_ESPNOW ? RADIO_MODE_ESPNOW : (uint8_t)e22_get_mode();
}
bool radio_transport_e22_ready(void) { return s_e22_ready; }
