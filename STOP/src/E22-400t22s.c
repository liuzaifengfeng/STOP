#include "E22-400t22s.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define E22_RX_BUFFER_SIZE       2048
#define E22_UART_EVENT_QUEUE_LEN 16
#define E22_RX_QUEUE_LEN         8
#define E22_RX_TASK_STACK        4096
#define E22_RX_TASK_PRIORITY     10
#define E22_DEFAULT_TIMEOUT      pdMS_TO_TICKS(1000)
#define E22_RESET_LOW_TIME       pdMS_TO_TICKS(20)
#define E22_BOOT_GUARD_TIME      pdMS_TO_TICKS(100)
#define E22_MODE_GUARD_TIME      pdMS_TO_TICKS(20)
#define E22_CONFIG_RETRY_DELAY   pdMS_TO_TICKS(100)
#define E22_CONFIG_READ_ATTEMPTS 3U
#define E22_CONFIG_REG_COUNT     9U
#define E22_CONFIG_RESPONSE_LEN  (3U + E22_CONFIG_REG_COUNT)
#define E22_PARSER_BUFFER_SIZE   256U

static const char *TAG = "E22-T22S";

/* ADDH, ADDL, NETID, REG0, REG1, REG2, REG3, CRYPT_H, CRYPT_L. */
static const uint8_t s_expected_config[E22_CONFIG_REG_COUNT] = {
    0x00, 0x00, 0x00, 0x62, 0x00, 0x17, 0x00, 0x00, 0x00,
};

static QueueHandle_t s_uart_event_queue;
static QueueHandle_t s_rx_queue;
static SemaphoreHandle_t s_operation_mutex;
static TaskHandle_t s_rx_task;
static volatile e22_mode_t s_mode = E22_MODE_UNINIT;
static volatile bool s_initialized;

typedef struct {
    uint8_t data[E22_PARSER_BUFFER_SIZE];
    size_t used;
} e22_parser_t;

static e22_parser_t s_parser;

static TickType_t remaining_ticks(TickType_t start, TickType_t timeout)
{
    if (timeout == portMAX_DELAY) {
        return portMAX_DELAY;
    }

    TickType_t elapsed = xTaskGetTickCount() - start;
    return elapsed >= timeout ? 0 : timeout - elapsed;
}

static esp_err_t wait_aux_high(TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();

    do {
        if (gpio_get_level(E22_PIN_AUX) != 0) {
            return ESP_OK;
        }
        if (remaining_ticks(start, timeout_ticks) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (true);

    ESP_LOGE(TAG, "AUX busy timeout");
    return ESP_ERR_TIMEOUT;
}

/*
 * NRST 释放瞬间 AUX 可能仍残留为高电平，不能立即将其视为启动完成。
 * 在固定保护期内观察 AUX：若见到低电平，则必须等它重新变高；如果
 * 始终为高，也会完整等待保护期后才继续，兼容很短或未被采样到的低脉冲。
 */
static esp_err_t wait_module_boot(TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    bool saw_aux_low = gpio_get_level(E22_PIN_AUX) == 0;

    while (remaining_ticks(start, timeout_ticks) != 0) {
        int level = gpio_get_level(E22_PIN_AUX);
        if (level == 0) {
            saw_aux_low = true;
        }

        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= E22_BOOT_GUARD_TIME && level != 0) {
            ESP_LOGI(TAG, "Module reset complete (AUX low observed: %s)",
                     saw_aux_low ? "yes" : "no");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGE(TAG, "Module startup timeout: AUX=%d, M0=%d, M1=%d",
             gpio_get_level(E22_PIN_AUX), gpio_get_level(E22_PIN_M0),
             gpio_get_level(E22_PIN_M1));
    return ESP_ERR_TIMEOUT;
}

static esp_err_t set_mode_locked(e22_mode_t mode, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    int m0;
    int m1;

    switch (mode) {
    case E22_MODE_TRANSMIT:
        m1 = 0;
        m0 = 0;
        break;
    case E22_MODE_WOR:
        m1 = 0;
        m0 = 1;
        break;
    case E22_MODE_CONFIG:
        m1 = 1;
        m0 = 0;
        break;
    case E22_MODE_SLEEP:
        m1 = 1;
        m0 = 1;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = wait_aux_high(remaining_ticks(start, timeout_ticks));
    if (err != ESP_OK) {
        return err;
    }

    gpio_set_level(E22_PIN_M1, m1);
    gpio_set_level(E22_PIN_M0, m0);
    /* 给模块足够时间采样 M0/M1，避免 AUX 的旧高电平造成误判。 */
    vTaskDelay(E22_MODE_GUARD_TIME);

    TickType_t remaining = remaining_ticks(start, timeout_ticks);
    if (remaining == 0) {
        return ESP_ERR_TIMEOUT;
    }
    err = wait_aux_high(remaining);
    if (err == ESP_OK) {
        s_mode = mode;
    }
    return err;
}

static esp_err_t uart_read_exact(uint8_t *data, size_t len, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    size_t offset = 0;

    while (offset < len) {
        TickType_t remaining = remaining_ticks(start, timeout_ticks);
        if (remaining == 0) {
            return ESP_ERR_TIMEOUT;
        }

        int read_len = uart_read_bytes(E22_UART_PORT, data + offset, len - offset, remaining);
        if (read_len < 0) {
            return ESP_FAIL;
        }
        offset += (size_t)read_len;
    }
    return ESP_OK;
}

static esp_err_t uart_command(const uint8_t *command, size_t command_len,
                              uint8_t *response, size_t response_len)
{
    uart_flush_input(E22_UART_PORT);
    xQueueReset(s_uart_event_queue);

    int written = uart_write_bytes(E22_UART_PORT, command, command_len);
    if (written != (int)command_len) {
        return ESP_FAIL;
    }

    esp_err_t err = uart_wait_tx_done(E22_UART_PORT, E22_DEFAULT_TIMEOUT);
    if (err != ESP_OK) {
        return err;
    }
    return uart_read_exact(response, response_len, E22_DEFAULT_TIMEOUT);
}

static esp_err_t read_config_once(uint8_t config[E22_CONFIG_REG_COUNT])
{
    const uint8_t command[] = {0xC1, 0x00, E22_CONFIG_REG_COUNT};
    uint8_t response[E22_CONFIG_RESPONSE_LEN];
    esp_err_t err = uart_command(command, sizeof(command), response, sizeof(response));
    if (err != ESP_OK) {
        return err;
    }

    if (response[0] != 0xC1 || response[1] != 0x00 ||
        response[2] != E22_CONFIG_REG_COUNT) {
        ESP_LOGE(TAG, "Invalid configuration response header: %02X %02X %02X",
                 response[0], response[1], response[2]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    memcpy(config, response + 3, E22_CONFIG_REG_COUNT);
    return ESP_OK;
}

static esp_err_t read_config(uint8_t config[E22_CONFIG_REG_COUNT])
{
    esp_err_t last_err = ESP_FAIL;

    for (unsigned attempt = 1; attempt <= E22_CONFIG_READ_ATTEMPTS; ++attempt) {
        esp_err_t aux_err = wait_aux_high(E22_DEFAULT_TIMEOUT);
        if (aux_err == ESP_OK) {
            last_err = read_config_once(config);
        } else {
            last_err = aux_err;
        }

        if (last_err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration read succeeded on attempt %u/%u", attempt,
                     E22_CONFIG_READ_ATTEMPTS);
            return ESP_OK;
        }

        ESP_LOGW(TAG,
                 "Configuration read attempt %u/%u failed: %s "
                 "(AUX=%d, M0=%d, M1=%d)",
                 attempt, E22_CONFIG_READ_ATTEMPTS, esp_err_to_name(last_err),
                 gpio_get_level(E22_PIN_AUX), gpio_get_level(E22_PIN_M0),
                 gpio_get_level(E22_PIN_M1));
        if (attempt < E22_CONFIG_READ_ATTEMPTS) {
            uart_flush_input(E22_UART_PORT);
            xQueueReset(s_uart_event_queue);
            vTaskDelay(E22_CONFIG_RETRY_DELAY);
        }
    }

    ESP_LOGE(TAG, "Configuration read failed after %u attempts",
             E22_CONFIG_READ_ATTEMPTS);
    return last_err;
}

static esp_err_t write_config(void)
{
    uint8_t command[3 + E22_CONFIG_REG_COUNT] = {
        0xC0, 0x00, E22_CONFIG_REG_COUNT,
    };
    uint8_t response[E22_CONFIG_RESPONSE_LEN];
    memcpy(command + 3, s_expected_config, E22_CONFIG_REG_COUNT);

    esp_err_t err = uart_command(command, sizeof(command), response, sizeof(response));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Configuration write failed: %s", esp_err_to_name(err));
        return err;
    }

    if (response[0] != 0xC1 || response[1] != 0x00 ||
        response[2] != E22_CONFIG_REG_COUNT ||
        memcmp(response + 3, s_expected_config, E22_CONFIG_REG_COUNT) != 0) {
        ESP_LOGE(TAG, "Configuration write acknowledgement mismatch");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return wait_aux_high(E22_DEFAULT_TIMEOUT);
}

static esp_err_t configure_module(void)
{
    uint8_t current[E22_CONFIG_REG_COUNT];
    esp_err_t err = set_mode_locked(E22_MODE_CONFIG, E22_DEFAULT_TIMEOUT);
    if (err != ESP_OK) {
        return err;
    }

    err = read_config(current);
    if (err != ESP_OK) {
        return err;
    }

    if (memcmp(current, s_expected_config, sizeof(current)) != 0) {
        ESP_LOGW(TAG, "Module parameters differ; writing project defaults");
        err = write_config();
        if (err != ESP_OK) {
            return err;
        }
        err = read_config(current);
        if (err != ESP_OK) {
            return err;
        }
        if (memcmp(current, s_expected_config, sizeof(current)) != 0) {
            ESP_LOGE(TAG, "Configuration read-back mismatch");
            return ESP_ERR_INVALID_RESPONSE;
        }
    } else {
        ESP_LOGI(TAG, "Module parameters already match project defaults");
    }

    ESP_LOGI(TAG, "Radio: 433.125 MHz, 2.4 kbps, 22 dBm, transparent mode");
    return set_mode_locked(E22_MODE_TRANSMIT, E22_DEFAULT_TIMEOUT);
}

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0 ? (uint16_t)((crc << 1) ^ 0x1021U)
                                      : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void parser_drop_prefix(size_t count)
{
    if (count >= s_parser.used) {
        s_parser.used = 0;
        return;
    }
    memmove(s_parser.data, s_parser.data + count, s_parser.used - count);
    s_parser.used -= count;
}

static void parser_align_magic(void)
{
    size_t start = 0;
    while (start + 1 < s_parser.used &&
           (s_parser.data[start] != E22_FRAME_MAGIC_0 ||
            s_parser.data[start + 1] != E22_FRAME_MAGIC_1)) {
        ++start;
    }

    if (start + 1 < s_parser.used) {
        parser_drop_prefix(start);
    } else if (s_parser.used > 0 &&
               s_parser.data[s_parser.used - 1] == E22_FRAME_MAGIC_0) {
        s_parser.data[0] = E22_FRAME_MAGIC_0;
        s_parser.used = 1;
    } else {
        s_parser.used = 0;
    }
}

static void parser_process(void)
{
    while (s_parser.used >= 2) {
        parser_align_magic();
        if (s_parser.used < 3) {
            return;
        }

        uint8_t payload_len = s_parser.data[2];
        if (payload_len == 0 || payload_len > E22_MAX_PAYLOAD_LEN) {
            ESP_LOGW(TAG, "Discarding frame with invalid length: %u", payload_len);
            parser_drop_prefix(1);
            continue;
        }

        size_t frame_len = (size_t)payload_len + E22_FRAME_OVERHEAD;
        if (s_parser.used < frame_len) {
            return;
        }

        uint16_t expected_crc = crc16_ccitt_false(s_parser.data + 2,
                                                   (size_t)payload_len + 1);
        uint16_t received_crc = ((uint16_t)s_parser.data[3 + payload_len] << 8) |
                                s_parser.data[4 + payload_len];
        if (expected_crc != received_crc) {
            ESP_LOGW(TAG, "CRC mismatch: expected %04X, received %04X",
                     expected_crc, received_crc);
            parser_drop_prefix(1);
            continue;
        }

        e22_rx_msg_t message = {.len = payload_len};
        memcpy(message.data, s_parser.data + 3, payload_len);
        if (xQueueSend(s_rx_queue, &message, 0) != pdTRUE) {
            ESP_LOGW(TAG, "RX queue full; dropping frame");
        }
        parser_drop_prefix(frame_len);
    }
}

static void parser_feed(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (s_parser.used == sizeof(s_parser.data)) {
            ESP_LOGW(TAG, "Parser buffer overflow; resynchronizing");
            s_parser.used = 0;
        }
        s_parser.data[s_parser.used++] = data[i];
        parser_process();
    }
}

static void e22_rx_task(void *parameter)
{
    (void)parameter;
    uart_event_t event;
    uint8_t buffer[256];

    ESP_LOGI(TAG, "UART receive task started");
    while (true) {
        if (xQueueReceive(s_uart_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
        case UART_DATA: {
            size_t remaining = event.size;
            while (remaining > 0) {
                size_t requested = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
                int read_len = uart_read_bytes(E22_UART_PORT, buffer, requested,
                                               pdMS_TO_TICKS(100));
                if (read_len <= 0) {
                    break;
                }
                parser_feed(buffer, (size_t)read_len);
                remaining -= (size_t)read_len;
            }
            break;
        }
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "UART receive overflow; flushing input");
            uart_flush_input(E22_UART_PORT);
            xQueueReset(s_uart_event_queue);
            s_parser.used = 0;
            break;
        case UART_BREAK:
        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART error event: %d", event.type);
            s_parser.used = 0;
            break;
        default:
            break;
        }
    }
}

static void release_resources(void)
{
    if (s_rx_task != NULL) {
        vTaskDelete(s_rx_task);
        s_rx_task = NULL;
    }
    uart_driver_delete(E22_UART_PORT);
    s_uart_event_queue = NULL;
    if (s_rx_queue != NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
    if (s_operation_mutex != NULL) {
        vSemaphoreDelete(s_operation_mutex);
        s_operation_mutex = NULL;
    }
    s_mode = E22_MODE_UNINIT;
    s_initialized = false;
}

esp_err_t e22_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_rx_queue = xQueueCreate(E22_RX_QUEUE_LEN, sizeof(e22_rx_msg_t));
    s_operation_mutex = xSemaphoreCreateMutex();
    if (s_rx_queue == NULL || s_operation_mutex == NULL) {
        release_resources();
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << E22_PIN_NRST) | (1ULL << E22_PIN_M0) |
                        (1ULL << E22_PIN_M1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t aux_config = {
        .pin_bit_mask = 1ULL << E22_PIN_AUX,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err == ESP_OK) {
        err = gpio_config(&aux_config);
    }
    if (err != ESP_OK) {
        release_resources();
        return err;
    }

    /* 在复位释放前就固定为配置模式，使模块从启动时便采样到正确模式。 */
    gpio_set_level(E22_PIN_M0, 0);
    gpio_set_level(E22_PIN_M1, 1);
    gpio_set_level(E22_PIN_NRST, 1);

    uart_config_t uart_config = {
        .baud_rate = E22_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    err = uart_param_config(E22_UART_PORT, &uart_config);
    if (err == ESP_OK) {
        err = uart_set_pin(E22_UART_PORT, E22_PIN_UART_TX, E22_PIN_UART_RX,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) {
        err = uart_driver_install(E22_UART_PORT, E22_RX_BUFFER_SIZE, 0,
                                  E22_UART_EVENT_QUEUE_LEN, &s_uart_event_queue, 0);
    }
    if (err != ESP_OK) {
        release_resources();
        return err;
    }

    gpio_set_level(E22_PIN_NRST, 0);
    vTaskDelay(E22_RESET_LOW_TIME);
    gpio_set_level(E22_PIN_NRST, 1);
    err = wait_module_boot(E22_DEFAULT_TIMEOUT);
    if (err == ESP_OK) {
        err = configure_module();
    }
    if (err != ESP_OK) {
        release_resources();
        return err;
    }

    uart_flush_input(E22_UART_PORT);
    xQueueReset(s_uart_event_queue);
    memset(&s_parser, 0, sizeof(s_parser));
    if (xTaskCreate(e22_rx_task, "e22_rx", E22_RX_TASK_STACK, NULL,
                    E22_RX_TASK_PRIORITY, &s_rx_task) != pdPASS) {
        release_resources();
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "E22-400T22S initialized on UART%d", E22_UART_PORT);
    return ESP_OK;
}

esp_err_t e22_send(const uint8_t *data, size_t len, TickType_t timeout_ticks)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0 || len > E22_MAX_PAYLOAD_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t start = xTaskGetTickCount();
    if (xSemaphoreTake(s_operation_mutex, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ESP_OK;
    if (s_mode != E22_MODE_TRANSMIT) {
        err = ESP_ERR_INVALID_STATE;
        goto done;
    }

    uint8_t frame[E22_FRAME_OVERHEAD + E22_MAX_PAYLOAD_LEN];
    frame[0] = E22_FRAME_MAGIC_0;
    frame[1] = E22_FRAME_MAGIC_1;
    frame[2] = (uint8_t)len;
    memcpy(frame + 3, data, len);
    uint16_t crc = crc16_ccitt_false(frame + 2, len + 1);
    frame[3 + len] = (uint8_t)(crc >> 8);
    frame[4 + len] = (uint8_t)crc;
    size_t frame_len = len + E22_FRAME_OVERHEAD;

    err = wait_aux_high(remaining_ticks(start, timeout_ticks));
    if (err != ESP_OK) {
        goto done;
    }

    int written = uart_write_bytes(E22_UART_PORT, frame, frame_len);
    if (written != (int)frame_len) {
        err = ESP_FAIL;
        goto done;
    }

    err = uart_wait_tx_done(E22_UART_PORT, remaining_ticks(start, timeout_ticks));
    if (err != ESP_OK) {
        goto done;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    TickType_t remaining = remaining_ticks(start, timeout_ticks);
    err = remaining == 0 ? ESP_ERR_TIMEOUT : wait_aux_high(remaining);

done:
    xSemaphoreGive(s_operation_mutex);
    return err;
}

esp_err_t e22_receive(e22_rx_msg_t *msg, TickType_t timeout_ticks)
{
    if (msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return xQueueReceive(s_rx_queue, msg, timeout_ticks) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

esp_err_t e22_sleep(uint32_t sleep_time_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_operation_mutex, E22_DEFAULT_TIMEOUT) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = s_mode == E22_MODE_SLEEP
                        ? ESP_OK
                        : set_mode_locked(E22_MODE_SLEEP, E22_DEFAULT_TIMEOUT);
    xSemaphoreGive(s_operation_mutex);
    if (err != ESP_OK || sleep_time_ms == 0) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(sleep_time_ms));
    return e22_wakeup();
}

esp_err_t e22_wakeup(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_operation_mutex, E22_DEFAULT_TIMEOUT) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = s_mode == E22_MODE_SLEEP
                        ? set_mode_locked(E22_MODE_TRANSMIT, E22_DEFAULT_TIMEOUT)
                        : ESP_OK;
    xSemaphoreGive(s_operation_mutex);
    return err;
}

e22_mode_t e22_get_mode(void)
{
    return s_mode;
}
