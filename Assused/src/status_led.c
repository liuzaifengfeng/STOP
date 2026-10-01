#include "status_led.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "receiver_safety.h"

/* 主色亮度：0~255；原值为 16，调大可提高亮度。其余色分量按比例缩放。 */
#define LED_BRIGHTNESS 128U
#define LED_COLOR_ACCENT (LED_BRIGHTNESS * 5U / 16U)
#define LED_COLOR_BLUE_BLINK (LED_BRIGHTNESS * 12U / 16U)

static const char *TAG = "STATUS_LED";
static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;

static bool set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    /* WS2812 使用 GRB 顺序；10MHz 下每个 tick 为 100ns。 */
    uint8_t grb[3] = {green, red, blue};
    rmt_transmit_config_t tx = {.loop_count = 0};
    esp_err_t err = rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx);
    if (err == ESP_OK) {
        /* 该 API 参数是毫秒，不能先用 pdMS_TO_TICKS 转成 tick。 */
        err = rmt_tx_wait_all_done(s_channel, 100);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 transmit failed: %s; stop LED updates",
                 esp_err_to_name(err));
    }
    return err == ESP_OK;
}

static void led_task(void *arg)
{
    (void)arg;
    bool blink = false;
    while (true) {
        blink = !blink;
        uint8_t red = 0, green = 0, blue = 0;
        uint32_t hold_ms = receiver_safety_key_hold_ms();
        if (hold_ms > 0 && receiver_safety_state() != RECEIVER_ARMED) {
            if (hold_ms >= 8000) { red = blink ? 12 : 4; blue = 16; } /* 紫：已达到配对时长 */
            else if (hold_ms >= 3000) { red = 16; green = 10; } /* 黄：可松开尝试恢复 */
            else { green = 8; blue = 16; } /* 青：检测到按键按下 */
        } else switch (receiver_safety_state()) {
        case RECEIVER_ARMED: green = LED_COLOR_ACCENT; break;
        case RECEIVER_PAIRING: blue = blink ? LED_COLOR_BLUE_BLINK : 0; break;
        case RECEIVER_UNPAIRED: blue = blink ? LED_COLOR_ACCENT : 0; break;
        case RECEIVER_TRIPPED: red = LED_COLOR_ACCENT; green = 0; break;
        case RECEIVER_LINK_LOST: red = blink ? LED_COLOR_ACCENT : 0; break;
        default: red = LED_COLOR_ACCENT; break;
        }
        if (!set_color(red, green, blue)) break;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelete(NULL);
}

esp_err_t status_led_start(void)
{
    rmt_tx_channel_config_t channel = {
        .gpio_num = GPIO_NUM_20,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    esp_err_t err = rmt_new_tx_channel(&channel, &s_channel);
    if (err != ESP_OK) return err;
    rmt_bytes_encoder_config_t encoder = {
        .bit0 = {.level0 = 1, .duration0 = 4, .level1 = 0, .duration1 = 9},
        .bit1 = {.level0 = 1, .duration0 = 8, .level1 = 0, .duration1 = 5},
        .flags.msb_first = 1,
    };
    err = rmt_new_bytes_encoder(&encoder, &s_encoder);
    if (err != ESP_OK) return err;
    err = rmt_enable(s_channel);
    if (err != ESP_OK) return err;
    if (xTaskCreate(led_task, "status_led", 3072, NULL, 2, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "WS2812 status LED started on GPIO20");
    return ESP_OK;
}
