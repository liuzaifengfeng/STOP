#include "controller_safety.h"

#include <string.h>
#include "radio_transport.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "safety_radio_frame.h"

#define ESTOP_PIN GPIO_NUM_6
#define HEARTBEAT_MS 200U
#define SAFETY_TX_TIMEOUT_MS 500U
#define DIAGNOSTIC_LISTEN_MS 10000U
#define STOP_ACK_WAIT_MS 1200U

static const char *TAG = "CONTROLLER_SAFE";
static uint8_t s_self[6], s_peer[6], s_key[16];
static volatile bool s_paired;
static bool s_pair_pending;
static volatile bool s_maintenance;
static volatile bool s_shutdown;
static volatile bool s_task_stopped;
static volatile uint32_t s_listen_until;
static uint32_t s_boot_id, s_sequence, s_pair_request_ms;
static portMUX_TYPE s_sequence_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_stop_sequence;
static bool s_stop_waiting, s_stop_ack_seen, s_stop_verified;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void load_pair(void)
{
    nvs_handle_t nvs;
    if (nvs_open("tx_pair", NVS_READONLY, &nvs) != ESP_OK) return;
    size_t peer_len = 6, key_len = 16;
    s_paired = nvs_get_blob(nvs, "peer", s_peer, &peer_len) == ESP_OK &&
               peer_len == 6 && nvs_get_blob(nvs, "key", s_key, &key_len) == ESP_OK &&
               key_len == 16;
    nvs_close(nvs);
}

static bool save_pair(void)
{
    nvs_handle_t nvs;
    if (nvs_open("tx_pair", NVS_READWRITE, &nvs) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(nvs, "peer", s_peer, sizeof(s_peer));
    if (err == ESP_OK) err = nvs_set_blob(nvs, "key", s_key, sizeof(s_key));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

bool controller_safety_on_radio(const uint8_t *data, size_t len)
{
    if (!data || len < 4 || data[0] != 'S' || data[1] != 'R') return false;
    safety_radio_frame_t frame;
    if (!safety_radio_decode(data, len, s_paired ? s_key : NULL, &frame) ||
        memcmp(frame.destination, s_self, 6) != 0) return true;
    if (frame.type == SAFETY_RADIO_STOP_ACK) {
        portENTER_CRITICAL(&s_sequence_lock);
        /* 回执必须来自已配对设备，并对应本次启动、本次停机请求。 */
        if (s_paired && s_stop_waiting &&
            safety_radio_stop_ack_matches(&frame, s_peer, s_self, s_boot_id, s_stop_sequence)) {
            s_stop_ack_seen = true;
            if (frame.state == SAFETY_RADIO_ACK_VERIFIED_OFF) s_stop_verified = true;
        }
        portEXIT_CRITICAL(&s_sequence_lock);
        return true;
    }
    if (s_shutdown || !s_pair_pending || frame.type != SAFETY_RADIO_PAIR_ACCEPT ||
        (uint32_t)(now_ms() - s_pair_request_ms) > 30000U) return true;
    memcpy(s_peer, frame.source, 6);
    memcpy(s_key, frame.key, 16);
    if (save_pair()) {
        /* 同一个确认可能从两条链路和重发到达，只接受一次。 */
        s_pair_pending = false;
        s_paired = true;
        s_boot_id = esp_random();
        portENTER_CRITICAL(&s_sequence_lock);
        s_sequence = 0;
        portEXIT_CRITICAL(&s_sequence_lock);
        ESP_LOGI(TAG, "Receiver paired; awaiting local receiver arm");
    }
    return true;
}

static void send_frame(uint8_t type, uint8_t state)
{
    /* 低压停机锁存后，任何并发业务都不能再发使能心跳。 */
    if (s_shutdown && type == SAFETY_RADIO_HEARTBEAT) state = 0;
    safety_radio_frame_t frame = {.type = type, .state = state};
    memcpy(frame.source, s_self, 6);
    bool authenticated = type >= SAFETY_RADIO_HEARTBEAT;
    if (authenticated) memcpy(frame.destination, s_peer, 6);
    frame.boot_id = s_boot_id;
    portENTER_CRITICAL(&s_sequence_lock);
    frame.sequence = ++s_sequence;
    if (type == SAFETY_RADIO_STOP_REQUEST) {
        /* 在 UART 写入前登记，避免快速回执先于等待状态建立。 */
        s_stop_sequence = frame.sequence;
        s_stop_waiting = true;
    }
    portEXIT_CRITICAL(&s_sequence_lock);
    uint8_t bytes[SAFETY_RADIO_PAIR_LEN];
    size_t n = safety_radio_encode(&frame,
                                  authenticated ? s_key : NULL,
                                  bytes, sizeof(bytes));
    if (n) {
        esp_err_t err = radio_transport_send(bytes, n, pdMS_TO_TICKS(SAFETY_TX_TIMEOUT_MS));
        if (err != ESP_OK)
            ESP_LOGW(TAG, "Safety radio send failed: type=%u, %s, link=%s",
                     type, esp_err_to_name(err), radio_transport_name());
    }
}

static void controller_task(void *arg)
{
    (void)arg;
    uint32_t last_pair_ms = 0;
    bool previous_safe = false;
    while (true) {
        if (s_shutdown) {
            s_task_stopped = true;
            vTaskDelete(NULL);
        }
        /* 常闭触点：正常闭合把 KEY 接 3.3V；断开时内部下拉为 0。 */
        bool safe = !s_maintenance && gpio_get_level(ESTOP_PIN) == 1;
        vTaskDelay(pdMS_TO_TICKS(20));
        safe = safe && !s_maintenance && gpio_get_level(ESTOP_PIN) == 1;
        uint32_t listen_until = s_listen_until;
        if (listen_until != 0U) {
            if ((int32_t)(listen_until - now_ms()) > 0) {
                previous_safe = false;
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            s_listen_until = 0;
            ESP_LOGI(TAG, "Diagnostic receive window closed; safety radio resumed");
        }
        /* 急停正常且已配对时优先心跳；其余情况每 5 秒发一次配对请求。 */
        if ((!s_paired || !safe) &&
            (last_pair_ms == 0 || (uint32_t)(now_ms() - last_pair_ms) >= 5000U)) {
            s_pair_request_ms = now_ms();
            s_pair_pending = true;
            send_frame(SAFETY_RADIO_PAIR_REQUEST, 0);
            last_pair_ms = s_pair_request_ms;
        }
        if (s_paired) {
            if (!safe && previous_safe) {
                /* 急停沿额外连发 3 帧，仍由接收端失联超时兜底。 */
                for (unsigned i = 0; i < 3; ++i)
                    send_frame(SAFETY_RADIO_HEARTBEAT, 0);
            }
            send_frame(SAFETY_RADIO_HEARTBEAT, safe ? 1 : 0);
        }
        previous_safe = safe;
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_MS));
    }
}

esp_err_t controller_safety_start(bool radio_ready)
{
    if (!radio_ready) return ESP_ERR_INVALID_STATE;
    (void)esp_read_mac(s_self, ESP_MAC_BT);
    s_boot_id = esp_random();
    load_pair();
    gpio_config_t key = {
        .pin_bit_mask = 1ULL << ESTOP_PIN, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    esp_err_t err = gpio_config(&key);
    if (err != ESP_OK) return err;
    return xTaskCreate(controller_task, "controller_safe", 4096, NULL, 8, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

void controller_safety_set_maintenance(bool active)
{
    s_maintenance = active;
    if (active) {
        if (s_paired) {
            for (unsigned i = 0; i < 3; ++i)
                send_frame(SAFETY_RADIO_HEARTBEAT, 0);
        }
        /* 即使急停报文全部丢失，也等待接收端 800ms 失联截止。 */
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

esp_err_t controller_safety_begin_receive_test(void)
{
    if (s_maintenance || s_shutdown) return ESP_ERR_INVALID_STATE;
    /* 即便上位机读到的输出状态过时，先发 STOP，再静默；接收端保持失联断电锁存。 */
    s_listen_until = now_ms() + DIAGNOSTIC_LISTEN_MS;
    if (s_paired) send_frame(SAFETY_RADIO_HEARTBEAT, 0);
    ESP_LOGW(TAG, "Diagnostic receive window OPEN for 10 s; safety heartbeat paused");
    return ESP_OK;
}

bool controller_safety_shutdown(void)
{
    s_shutdown = true;
    s_maintenance = true;
    /* 等正在发送的帧完成，避免最后一帧正常心跳覆盖 STOP。 */
    while (!s_task_stopped) vTaskDelay(pdMS_TO_TICKS(20));
    if (s_paired) {
        for (unsigned i = 0; i < 3; ++i) {
            /* 保留旧版本能识别的失能心跳，再发需要回执的新请求。 */
            send_frame(SAFETY_RADIO_HEARTBEAT, 0);
            send_frame(SAFETY_RADIO_STOP_REQUEST, 0);
            uint32_t start = now_ms();
            bool verified = false;
            do {
                portENTER_CRITICAL(&s_sequence_lock);
                verified = s_stop_verified;
                portEXIT_CRITICAL(&s_sequence_lock);
                if (verified) break;
                vTaskDelay(pdMS_TO_TICKS(20));
            } while ((uint32_t)(now_ms() - start) < STOP_ACK_WAIT_MS);
            if (verified) break;
            ESP_LOGW(TAG, "STOP ACK attempt %u/3: output not verified OFF", i + 1);
        }
        portENTER_CRITICAL(&s_sequence_lock);
        bool verified = s_stop_verified;
        bool seen = s_stop_ack_seen;
        s_stop_waiting = false;
        portEXIT_CRITICAL(&s_sequence_lock);
        if (verified) {
            ESP_LOGI(TAG, "STOP ACK authenticated: receiver driver OFF, VOUT <2 V");
            return true;
        }
        ESP_LOGW(TAG, "%s; keep silent 900 ms for link-loss trip before sleep",
                 seen ? "STOP ACK received but VOUT not verified OFF" : "No matching STOP ACK");
        vTaskDelay(pdMS_TO_TICKS(900));
    }
    return false;
}

bool controller_safety_receive_test_active(void)
{
    uint32_t until = s_listen_until;
    return until != 0U && (int32_t)(until - now_ms()) > 0;
}

uint8_t controller_safety_pair_status(uint8_t peer[6])
{
    if (peer != NULL) {
        if (s_paired) memcpy(peer, s_peer, 6);
        else memset(peer, 0, 6);
    }
    return (s_paired ? 1U : 0U) |
           (gpio_get_level(ESTOP_PIN) == 1 ? 2U : 0U) |
           (s_maintenance ? 4U : 0U);
}
