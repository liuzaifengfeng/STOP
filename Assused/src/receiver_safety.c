#include "receiver_safety.h"

#include <string.h>
#include "radio_transport.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ina226_monitor.h"
#include "nvs.h"
#include "ota_service.h"
#include "safety_radio_frame.h"
#include "vin_monitor.h"

#define ENABLE_PIN GPIO_NUM_21
#define LOCAL_KEY GPIO_NUM_6
#define HEARTBEAT_TIMEOUT_MS 800U
#define PAIR_WINDOW_MS 30000U
#define ARM_PRESS_MS 3000U
#define PAIR_PRESS_MS 8000U
#define SOFTWARE_CURRENT_MA 5500
/* 临时关闭空载电容残压检查；改回 1 可恢复断电后 VOUT >2V 的故障保护。 */
#define OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED 0

static const char *TAG = "RECEIVER_SAFE";
static uint8_t s_self[6], s_peer[6], s_key[16];
static volatile bool s_paired, s_radio_ready, s_enabled, s_remote_safe;
static volatile uint8_t s_state = RECEIVER_UNPAIRED;
static volatile uint32_t s_boot_id, s_seq, s_last_ms, s_good;
static volatile uint32_t s_pair_until;
#if OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED
static volatile uint32_t s_off_since;
#endif
static volatile uint32_t s_enable_since;
static volatile uint32_t s_key_press_ms;
static volatile bool s_key_pressed;
/* 上电默认跟随控制端释放状态；手动断电或硬件故障可撤销自动使能。 */
static volatile bool s_auto_enable_allowed = true;
/* 保留第一次故障的原因和触发读数，避免断电后的正常读数掩盖原因。 */
static const char *s_fault_reason;
static int32_t s_fault_value, s_fault_limit;
static esp_err_t s_fault_error;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
typedef struct {
    safety_radio_frame_t frame;
    uint8_t key[SAFETY_RADIO_KEY_LEN];
} stop_ack_job_t;
static QueueHandle_t s_stop_ack_queue;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static const char *state_name(uint8_t state)
{
    switch (state) {
    case RECEIVER_UNPAIRED: return "UNPAIRED";
    case RECEIVER_TRIPPED: return "TRIPPED";
    case RECEIVER_ARMED: return "ARMED";
    case RECEIVER_LINK_LOST: return "LINK_LOST";
    case RECEIVER_HARDWARE_FAULT: return "HARDWARE_FAULT";
    case RECEIVER_PAIRING: return "PAIRING";
    default: return "UNKNOWN";
    }
}

/* 串口监视器中即使错过开机信息，也能看到当前安全条件和传感器读数。 */
static void log_runtime_status(void)
{
    uint32_t last_ms = s_last_ms;
    uint32_t pair_until = s_pair_until;
    uint32_t age_ms = last_ms ? now_ms() - last_ms : 0;
    uint32_t pair_left_ms = pair_until && (int32_t)(pair_until - now_ms()) > 0
                            ? pair_until - now_ms() : 0;
    ESP_LOGI(TAG,
             "Status: state=%s, output=%s, paired=%s, radio=%s, key=%s, remote=%s, good=%lu/3, heartbeat_seen=%s, age=%lu ms, pair_window=%lu s",
             state_name(s_state), s_enabled ? "ON" : "OFF",
             s_paired ? "yes" : "no", s_radio_ready ? "OK" : "FAIL",
             gpio_get_level(LOCAL_KEY) == 0 ? "PRESSED" : "released",
             s_remote_safe ? "SAFE" : "STOP/unknown", (unsigned long)s_good,
             last_ms ? "yes" : "no", (unsigned long)age_ms,
             (unsigned long)((pair_left_ms + 999U) / 1000U));

    if (s_fault_reason) {
        ESP_LOGW(TAG, "Latched fault: reason=%s, trigger_value=%ld, limit=%ld, error=%s",
                 s_fault_reason, (long)s_fault_value, (long)s_fault_limit,
                 esp_err_to_name(s_fault_error));
    }
#if VIN_MONITOR_ENABLED
    int32_t vin_mv = 0;
    ina226_sample_t sample;
    esp_err_t vin_err = vin_monitor_read_mv(&vin_mv);
    esp_err_t ina_err = ina226_monitor_read(&sample);
    if (vin_err == ESP_OK && ina_err == ESP_OK) {
        ESP_LOGI(TAG, "Power: VIN=%ld mV, VOUT=%ld mV, current=%ld mA",
                 (long)vin_mv, (long)sample.bus_mv, (long)sample.current_ma);
    } else {
        ESP_LOGW(TAG, "Power read: VIN=%s, INA226=%s",
                 esp_err_to_name(vin_err), esp_err_to_name(ina_err));
    }
#else
    ina226_sample_t sample;
    esp_err_t ina_err = ina226_monitor_read(&sample);
    if (ina_err == ESP_OK)
        ESP_LOGI(TAG, "Power: VIN=DISABLED, VOUT=%ld mV, current=%ld mA",
                 (long)sample.bus_mv, (long)sample.current_ma);
    else ESP_LOGW(TAG, "Power read: VIN=DISABLED, INA226=%s", esp_err_to_name(ina_err));
#endif
}

static bool timed_out(uint32_t then, uint32_t ms)
{
    return (uint32_t)(now_ms() - then) >= ms;
}

static void trip(uint8_t reason)
{
    portENTER_CRITICAL(&s_state_lock);
    if (reason == RECEIVER_HARDWARE_FAULT)
        s_auto_enable_allowed = false;
#if OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED
    if (s_enabled) s_off_since = now_ms();
#endif
    gpio_set_level(ENABLE_PIN, 0);
    s_enabled = false;
    bool changed = s_state != reason;
    s_state = reason;
    portEXIT_CRITICAL(&s_state_lock);
    if (changed) ESP_LOGW(TAG, "Output OFF: %s (%u)", state_name(reason), reason);
}

/* 先撤销输出，再打印原因；数值单位由原因标明（mV/mA）。 */
static void hardware_fault(const char *reason, int32_t value, int32_t limit, esp_err_t error)
{
    portENTER_CRITICAL(&s_state_lock);
    bool first = s_fault_reason == NULL;
    if (first) {
        s_fault_value = value;
        s_fault_limit = limit;
        s_fault_error = error;
        s_fault_reason = reason;
    }
    portEXIT_CRITICAL(&s_state_lock);
    trip(RECEIVER_HARDWARE_FAULT);
    if (first) ESP_LOGE(TAG, "Hardware fault: reason=%s, trigger_value=%ld, limit=%ld, error=%s",
                        reason, (long)value, (long)limit, esp_err_to_name(error));
}

/* 回执发送独立于安全任务和收包任务，不因 AUX/I2C 超时延迟撤销使能。 */
static void stop_ack_task(void *arg)
{
    (void)arg;
    stop_ack_job_t job;
    while (true) {
        if (xQueueReceive(s_stop_ack_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        uint32_t start = now_ms();
        bool off = false, verified = false;
        do {
            /* 等待输出电容放电后再取样，不使用停机前的缓存值。 */
            vTaskDelay(pdMS_TO_TICKS(50));
            ina226_sample_t sample;
            bool low = ina226_monitor_read(&sample) == ESP_OK && sample.valid &&
                       sample.bus_mv < 2000;
            portENTER_CRITICAL(&s_state_lock);
            off = !s_enabled && !s_remote_safe &&
                  s_boot_id == job.frame.boot_id && s_seq == job.frame.sequence;
            portEXIT_CRITICAL(&s_state_lock);
            if (!off) break; /* 新命令已覆盖：不能确认旧请求。 */
            verified = low;
        } while (!verified && (uint32_t)(now_ms() - start) < 300U);
        if (!off) continue;
        job.frame.state = verified ? SAFETY_RADIO_ACK_VERIFIED_OFF : SAFETY_RADIO_ACK_DRIVER_OFF;
        uint8_t bytes[SAFETY_RADIO_BASE_LEN];
        size_t n = safety_radio_encode(&job.frame, job.key, bytes, sizeof(bytes));
        esp_err_t err = n ? radio_transport_send(bytes, n, pdMS_TO_TICKS(500)) : ESP_FAIL;
        ESP_LOGI(TAG, "STOP ACK: seq=%lu, driver=OFF, VOUT=%s, TX=%s",
                 (unsigned long)job.frame.sequence, verified ? "<2V" : "unconfirmed",
                 esp_err_to_name(err));
    }
}

static void schedule_stop_ack(const safety_radio_frame_t *request)
{
    if (!s_stop_ack_queue || !s_radio_ready) return;
    stop_ack_job_t job = {.frame = {
        .type = SAFETY_RADIO_STOP_ACK, .boot_id = request->boot_id,
        .sequence = request->sequence,
    }};
    memcpy(job.frame.source, s_self, 6);
    memcpy(job.frame.destination, request->source, 6);
    memcpy(job.key, s_key, sizeof(job.key));
    /* 单槽覆盖：重试时只处理最新请求，不堆积回执。 */
    xQueueOverwrite(s_stop_ack_queue, &job);
}

esp_err_t receiver_safety_open_pair_window(void)
{
    if (!s_radio_ready || ota_service_is_active()) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&s_state_lock);
    if (s_enabled || s_state == RECEIVER_HARDWARE_FAULT) {
        portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_auto_enable_allowed = false;
    gpio_set_level(ENABLE_PIN, 0);
    s_pair_until = now_ms() + PAIR_WINDOW_MS;
    s_state = RECEIVER_PAIRING;
    portEXIT_CRITICAL(&s_state_lock);
    ESP_LOGI(TAG, "Pair window OPEN for 30 s; output OFF");
    return ESP_OK;
}

uint32_t receiver_safety_key_hold_ms(void)
{
    uint32_t start = s_key_press_ms;
    return s_key_pressed && start ? now_ms() - start : 0;
}

static bool save_pair(void)
{
    nvs_handle_t nvs;
    if (nvs_open("rx_pair", NVS_READWRITE, &nvs) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(nvs, "peer", s_peer, sizeof(s_peer));
    if (err == ESP_OK) err = nvs_set_blob(nvs, "key", s_key, sizeof(s_key));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

static void load_pair(void)
{
    nvs_handle_t nvs;
    if (nvs_open("rx_pair", NVS_READONLY, &nvs) != ESP_OK) return;
    size_t peer_len = 6, key_len = 16;
    s_paired = nvs_get_blob(nvs, "peer", s_peer, &peer_len) == ESP_OK &&
               peer_len == 6 && nvs_get_blob(nvs, "key", s_key, &key_len) == ESP_OK &&
               key_len == 16;
    nvs_close(nvs);
}

bool receiver_safety_on_radio(const uint8_t *data, size_t len)
{
    if (!data || len < 4 || data[0] != 'S' || data[1] != 'R') return false;
    safety_radio_frame_t frame;
    if (!safety_radio_decode(data, len, s_paired ? s_key : NULL, &frame)) return true;
    const uint8_t broadcast[6] = {0};
    if (memcmp(frame.destination, s_self, 6) != 0 &&
        !(frame.type == SAFETY_RADIO_PAIR_REQUEST &&
          memcmp(frame.destination, broadcast, 6) == 0)) return true;

    if (frame.type == SAFETY_RADIO_PAIR_REQUEST) {
        if (!s_radio_ready || s_pair_until == 0 ||
            (int32_t)(s_pair_until - now_ms()) <= 0) return true;
        trip(RECEIVER_PAIRING);
        memcpy(s_peer, frame.source, 6);
        esp_fill_random(s_key, sizeof(s_key));
        if (!save_pair()) {
            hardware_fault("PAIR_SAVE_FAILED", 0, 0, ESP_FAIL);
            return true;
        }
        s_paired = true;
        s_boot_id = 0; s_seq = 0; s_good = 0;
        safety_radio_frame_t ack = {.type = SAFETY_RADIO_PAIR_ACCEPT};
        memcpy(ack.source, s_self, 6);
        memcpy(ack.destination, s_peer, 6);
        memcpy(ack.key, s_key, 16);
        ack.boot_id = esp_random();
        uint8_t bytes[SAFETY_RADIO_PAIR_LEN];
        size_t n = safety_radio_encode(&ack, NULL, bytes, sizeof(bytes));
        for (unsigned i = 0; i < 3 && n; ++i) {
            esp_err_t send_err = radio_transport_send(bytes, n, pdMS_TO_TICKS(600));
            ESP_LOGI(TAG, "Pair accept TX %u/3: %s, link=%s", i + 1,
                     esp_err_to_name(send_err), radio_transport_name());
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        s_pair_until = 0;
        trip(RECEIVER_TRIPPED);
        s_auto_enable_allowed = true;
        ESP_LOGI(TAG, "Pair saved; waiting for 3 safe heartbeats to enable automatically");
        return true;
    }
    bool stop_request = frame.type == SAFETY_RADIO_STOP_REQUEST;
    if ((frame.type != SAFETY_RADIO_HEARTBEAT && !stop_request) || !s_paired ||
        memcmp(frame.source, s_peer, 6) != 0 || frame.state > 1) return true;

    if (frame.boot_id != s_boot_id) {
        s_boot_id = frame.boot_id;
        s_seq = 0;
        s_good = 0;
        s_remote_safe = false;
        /* 新启动身份先断开并重新累计心跳，不能清除硬件故障锁存。 */
        trip(s_state == RECEIVER_HARDWARE_FAULT ? RECEIVER_HARDWARE_FAULT : RECEIVER_TRIPPED);
    }
    if (frame.sequence > s_seq) {
        if (timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS)) s_good = 0;
        s_seq = frame.sequence;
        s_last_ms = now_ms();
        s_remote_safe = frame.state == 1;
        s_good = s_remote_safe ? (s_good < 3 ? s_good + 1 : 3) : 0;
        if (!s_remote_safe) {
            /* 急停立即断开，释放后跟随有效心跳恢复；不覆盖手动禁止。 */
            trip(s_state == RECEIVER_HARDWARE_FAULT ? RECEIVER_HARDWARE_FAULT : RECEIVER_TRIPPED);
        }
    }
    /* 重复 STOP 可再次回执，但不刷新心跳；过期序号不产生回执。 */
    if (stop_request && frame.sequence == s_seq && !s_enabled && !s_remote_safe)
        schedule_stop_ack(&frame);
    return true;
}


/* 本地按键、BLE 调试和急停松开恢复共用同一组使能检查。 */
static esp_err_t confirm_enable(bool automatic)
{
    if (!s_paired || s_enabled || s_pair_until || s_state == RECEIVER_HARDWARE_FAULT)
    {
        ESP_LOGW(TAG, "Enable denied: paired=%d, output=%d, pair_window=%d, hardware_fault=%d",
                 s_paired, s_enabled, s_pair_until != 0, s_state == RECEIVER_HARDWARE_FAULT);
        log_runtime_status();
        return ESP_ERR_INVALID_STATE;
    }
    ina226_sample_t sample;
#if VIN_MONITOR_ENABLED
    int32_t vin_mv = 0;
#endif
    if (s_radio_ready && s_good >= 3 && s_remote_safe &&
        !timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS) &&
        !ota_service_is_active() &&
#if VIN_MONITOR_ENABLED
        vin_monitor_read_mv(&vin_mv) == ESP_OK &&
        vin_mv >= 9000 && vin_mv <= 30000 &&
#endif
        ina226_monitor_read(&sample) == ESP_OK &&
        sample.current_ma < SOFTWARE_CURRENT_MA &&
        sample.bus_mv < 2000 &&
        ina226_monitor_clear_alert() == ESP_OK) {
        portENTER_CRITICAL(&s_state_lock);
        bool still_safe = s_remote_safe && s_good >= 3 &&
                          !timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS) &&
                          (!automatic || s_auto_enable_allowed) &&
                          !s_enabled && s_pair_until == 0 && s_paired &&
                          s_radio_ready && !ota_service_is_active() &&
                          s_state != RECEIVER_HARDWARE_FAULT;
        if (still_safe) {
            gpio_set_level(ENABLE_PIN, 1);
            s_auto_enable_allowed = true;
            s_enabled = true;
            s_state = RECEIVER_ARMED;
            s_enable_since = now_ms();
        }
        portEXIT_CRITICAL(&s_state_lock);
        if (still_safe) ESP_LOGI(TAG, "%s", automatic ? "Output enabled automatically: controller released and 3 safe heartbeats" : "Output enabled after safety confirmation");
        else ESP_LOGW(TAG, "Enable denied: safety conditions changed during confirmation");
        return still_safe ? ESP_OK : ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGW(TAG, "Enable denied: inspect radio/heartbeat, VOUT <2 V, INA226/current and OTA (VIN ADC disabled in current build)");
        log_runtime_status();
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t receiver_safety_debug_output(bool enable)
{
    if (!enable) {
        portENTER_CRITICAL(&s_state_lock);
        s_auto_enable_allowed = false;
        portEXIT_CRITICAL(&s_state_lock);
        /* 断开命令不清除已锁存的硬件故障，也不延长配对窗口。 */
        trip(s_state == RECEIVER_HARDWARE_FAULT ? RECEIVER_HARDWARE_FAULT :
             s_pair_until ? RECEIVER_PAIRING :
             s_paired ? RECEIVER_TRIPPED : RECEIVER_UNPAIRED);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Temporary BLE debug enable requested");
    return confirm_enable(false);
}

static void receiver_task(void *arg)
{
    (void)arg;
    uint32_t press_start = 0, high_current_since = 0;
#if OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED
    uint32_t last_off_check = 0;
#endif
#if VIN_MONITOR_ENABLED
    uint32_t last_vin_check = 0;
#endif
    uint32_t last_status_log = now_ms();
    uint32_t last_resume_attempt = 0;
    bool pressed_before = false, candidate_pressed = false;
    bool pair_triggered = false, arm_hint_logged = false;
    uint32_t candidate_since = now_ms();
    ESP_LOGI(TAG, "Local key GPIO6 initial level=%d (%s)", gpio_get_level(LOCAL_KEY),
             gpio_get_level(LOCAL_KEY) == 0 ? "PRESSED" : "released");
    while (true) {
        bool raw_pressed = gpio_get_level(LOCAL_KEY) == 0;
        if (raw_pressed != candidate_pressed) {
            candidate_pressed = raw_pressed;
            candidate_since = now_ms();
        }
        if (candidate_pressed != pressed_before && timed_out(candidate_since, 40)) {
            pressed_before = candidate_pressed;
            if (pressed_before) {
                press_start = now_ms();
                s_key_press_ms = press_start;
                s_key_pressed = true;
                pair_triggered = false;
                arm_hint_logged = false;
                ESP_LOGI(TAG, "Local key PRESSED: GPIO6=0; 3 s arm / 8 s pair");
            } else {
                s_key_pressed = false;
                s_key_press_ms = 0;
                uint32_t held = now_ms() - press_start;
                ESP_LOGI(TAG, "Local key RELEASED: GPIO6=1, held=%lu ms", (unsigned long)held);
                if (pair_triggered) {
                    ESP_LOGI(TAG, "8 s pairing hold completed");
                } else if (!s_enabled && s_pair_until == 0 &&
                       held >= ARM_PRESS_MS && s_paired) {
                    (void)confirm_enable(false);
                } else if (!s_enabled && held >= ARM_PRESS_MS && !s_paired) {
                    ESP_LOGW(TAG, "Enable denied: not paired; hold local key 8 s to open pairing window");
                }
            }
        }

        if (pressed_before && !s_enabled) {
            uint32_t held = now_ms() - press_start;
            if (!arm_hint_logged && held >= ARM_PRESS_MS) {
                arm_hint_logged = true;
                ESP_LOGI(TAG, "Local key held 3 s: release before 8 s to arm if paired; keep holding to pair");
            }
            if (!pair_triggered && held >= PAIR_PRESS_MS) {
                pair_triggered = true;
                ESP_LOGI(TAG, "Local key held 8 s: requesting pair window");
                esp_err_t pair_err = receiver_safety_open_pair_window();
                if (pair_err != ESP_OK)
                    ESP_LOGW(TAG, "Pair window denied: %s", esp_err_to_name(pair_err));
            }
        }

        if (s_pair_until && (int32_t)(s_pair_until - now_ms()) <= 0) {
            s_pair_until = 0;
            if (s_state == RECEIVER_PAIRING)
                trip(s_paired ? RECEIVER_TRIPPED : RECEIVER_UNPAIRED);
        }
        /* 上电/链路恢复/急停释放：经认证控制端的连续安全心跳自动使能。 */
        if (s_auto_enable_allowed && !s_enabled) {
            if (ota_service_is_active()) {
                s_auto_enable_allowed = false;
                ESP_LOGW(TAG, "Auto enable cancelled: OTA active");
            } else if (timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS)) {
                s_good = 0;
            } else if (s_paired && s_remote_safe && s_good >= 3 && s_pair_until == 0 &&
                       (s_state == RECEIVER_TRIPPED || s_state == RECEIVER_LINK_LOST) &&
                       timed_out(last_resume_attempt, 500)) {
                last_resume_attempt = now_ms();
                if (confirm_enable(true) == ESP_OK) high_current_since = 0;
            }
        }
        if (s_enabled) {
            ina226_sample_t sample;
            esp_err_t sample_err = ESP_OK;
            if (!s_radio_ready || !s_remote_safe ||
                timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS)) {
                trip(RECEIVER_LINK_LOST);
            } else if (ota_service_is_active()) {
                hardware_fault("OTA_ACTIVE", 0, 0, ESP_OK);
            } else if ((sample_err = ina226_monitor_read(&sample)) != ESP_OK) {
                hardware_fault("INA226_READ_FAILED", 0, 0, sample_err);
            } else if (sample.current_ma >= 7000) {
                hardware_fault("OVERCURRENT_HARD_mA", sample.current_ma, 7000, ESP_OK);
            } else if (timed_out(s_enable_since, 500) && sample.bus_mv < 5000) {
                hardware_fault("OUTPUT_NOT_RISING_mV", sample.bus_mv, 5000, ESP_OK);
            } else if (sample.current_ma >= SOFTWARE_CURRENT_MA) {
                if (!high_current_since) high_current_since = now_ms();
                else if (timed_out(high_current_since, 200))
                    hardware_fault("OVERCURRENT_200ms_mA", sample.current_ma, SOFTWARE_CURRENT_MA, ESP_OK);
            } else high_current_since = 0;
#if VIN_MONITOR_ENABLED
            if (s_enabled && timed_out(last_vin_check, 500)) {
                int32_t vin_mv = 0;
                last_vin_check = now_ms();
                esp_err_t vin_err = vin_monitor_read_mv(&vin_mv);
                if (vin_err != ESP_OK)
                    hardware_fault("VIN_READ_FAILED", 0, 0, vin_err);
                else if (vin_mv < 8500)
                    hardware_fault("VIN_TOO_LOW_mV", vin_mv, 8500, ESP_OK);
                else if (vin_mv > 31000)
                    hardware_fault("VIN_TOO_HIGH_mV", vin_mv, 31000, ESP_OK);
            }
#endif
        }
#if OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED
        else if (s_state != RECEIVER_HARDWARE_FAULT && timed_out(s_off_since, 3000) &&
                   timed_out(last_off_check, 500)) {
            ina226_sample_t sample;
            last_off_check = now_ms();
            if (ina226_monitor_read(&sample) == ESP_OK && sample.bus_mv > 2000)
                hardware_fault("OUTPUT_STILL_ON_mV", sample.bus_mv, 2000, ESP_OK);
        }
#endif
        if (timed_out(last_status_log, 10000)) {
            last_status_log = now_ms();
            log_runtime_status();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t receiver_safety_start(bool radio_ready)
{
    gpio_set_level(ENABLE_PIN, 0);
    s_radio_ready = radio_ready;
#if OUTPUT_OFF_VOLTAGE_PROTECTION_ENABLED
    s_off_since = now_ms();
#else
    ESP_LOGW(TAG, "Output-off residual voltage protection temporarily DISABLED");
#endif
    (void)esp_read_mac(s_self, ESP_MAC_BT);
    load_pair();
    s_state = s_paired ? RECEIVER_TRIPPED : RECEIVER_UNPAIRED;
    gpio_config_t key = {
        .pin_bit_mask = 1ULL << LOCAL_KEY, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    esp_err_t err = gpio_config(&key);
    if (err != ESP_OK) return err;
    s_stop_ack_queue = xQueueCreate(1, sizeof(stop_ack_job_t));
    if (!s_stop_ack_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(stop_ack_task, "stop_ack", 4096, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(s_stop_ack_queue);
        s_stop_ack_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Safety ready: identity=%02x:%02x:%02x:%02x:%02x:%02x, paired=%s, radio=%s, state=%s, output=OFF",
             s_self[0], s_self[1], s_self[2], s_self[3], s_self[4], s_self[5],
             s_paired ? "yes" : "no", s_radio_ready ? "OK" : "FAIL",
             state_name(s_state));
    return xTaskCreate(receiver_task, "receiver_safe", 4096, NULL, 8, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

uint8_t receiver_safety_state(void) { return s_state; }
void receiver_safety_hardware_fault(void) { hardware_fault("STARTUP_INIT_FAILED", 0, 0, ESP_FAIL); }

uint8_t receiver_safety_pair_status(uint8_t peer[6])
{
    if (peer != NULL) {
        if (s_paired) memcpy(peer, s_peer, 6);
        else memset(peer, 0, 6);
    }
    return (s_paired ? 1U : 0U) |
           (s_remote_safe ? 2U : 0U) |
           (s_last_ms && !timed_out(s_last_ms, HEARTBEAT_TIMEOUT_MS) ? 4U : 0U) |
           (gpio_get_level(LOCAL_KEY) == 0 ? 8U : 0U) |
           (s_enabled ? 16U : 0U);
}
