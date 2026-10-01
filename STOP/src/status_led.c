#include "status_led.h"

#include "controller_safety.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_DATA GPIO_NUM_8
#define LED_POWER_GATE GPIO_NUM_9

/* 主色亮度：0~255；原值为 16，调大可提高亮度。其余色分量按比例缩放。 */
#define LED_BRIGHTNESS 128U
#define LED_COLOR_ACCENT (LED_BRIGHTNESS * 5U / 16U)
#define LED_COLOR_BLUE_BLINK (LED_BRIGHTNESS * 12U / 16U)

static const char *TAG = "STATUS_LED";
static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;
static bool s_radio_ready;

static bool set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t grb[3] = {green, red, blue};
    rmt_transmit_config_t tx = {.loop_count = 0};
    esp_err_t err = rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx);
    if (err == ESP_OK) err = rmt_tx_wait_all_done(s_channel, 100); /* 毫秒 */
    if (err != ESP_OK)
        ESP_LOGE(TAG, "WS2812 transmit failed: %s; stop LED updates", esp_err_to_name(err));
    return err == ESP_OK;
}

static void led_task(void *arg)
{
    (void)arg;
    bool blink = false;
    while (true) {
        blink = !blink;
        uint8_t red = 0, green = 0, blue = 0;
        uint8_t flags = s_radio_ready ? controller_safety_pair_status(NULL) : 0;
        if (!s_radio_ready) red = LED_BRIGHTNESS;                  /* 无线故障：红色 */
        else if (controller_safety_receive_test_active()) { green = LED_COLOR_ACCENT; blue = LED_BRIGHTNESS; } /* 反向联调：青色 */
        else if (flags & 4U) { red = LED_BRIGHTNESS; green = LED_COLOR_ACCENT; } /* OTA 维护：橙红 */
        else if (!(flags & 2U)) red = blink ? LED_BRIGHTNESS : 0;   /* 急停：红闪 */
        else if (!(flags & 1U)) blue = blink ? LED_COLOR_BLUE_BLINK : 0; /* 未配对：蓝闪 */
        else green = LED_BRIGHTNESS;                              /* 已配对且正常：绿色 */
        if (!set_color(red, green, blue)) break;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    gpio_set_level(LED_POWER_GATE, 1);
    vTaskDelete(NULL);
}

esp_err_t status_led_start(bool radio_ready)
{
    s_radio_ready = radio_ready;
    /* 网表 Q3 为 P 沟道高边开关：GPIO9 拉低才给 LED1 供电。 */
    gpio_set_level(LED_POWER_GATE, 1);
    gpio_config_t power = {
        .pin_bit_mask = 1ULL << LED_POWER_GATE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    esp_err_t err = gpio_config(&power);
    if (err != ESP_OK) return err;
    gpio_set_level(LED_POWER_GATE, 0);

    rmt_tx_channel_config_t channel = {
        .gpio_num = LED_DATA,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    err = rmt_new_tx_channel(&channel, &s_channel);
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
    ESP_LOGI(TAG, "WS2812 ready: data=GPIO8, power gate=GPIO9 (low=ON)");
    return ESP_OK;
}
