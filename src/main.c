#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "web_ota.h"
#include "adc_monitor.h"

#define BLINK_GPIO GPIO_NUM_22 // GPIO_NUM_22 is the BUZZER pin

static const char *TAG = "main";

//蜂鸣器任务（物理心跳）
static void blink_task(void *pvParameter)
{
    uint32_t boot_count = 0;
    int led_state = 1;

    // 初始状态翻转测试
    for (int i = 0; i < 4; i++) {
        led_state = !led_state;
        gpio_set_level(BLINK_GPIO, led_state);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    while (1) {
        ESP_LOGI(TAG, "System running normally... counter: %lu, LED: %d", ++boot_count, led_state);

        if (boot_count % 60 == 0) {
            led_state = 0;
            gpio_set_level(BLINK_GPIO, led_state);
            vTaskDelay(pdMS_TO_TICKS(200));
            led_state = 1;
            gpio_set_level(BLINK_GPIO, led_state);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C6 boot successful! Web OTA enabled.");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化 NVS (WiFi 和 OTA 标志位强依赖 NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化硬件引脚
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // 3. 创建业务任务
    xTaskCreate(blink_task, "blink_task", 4096, NULL, 5, NULL);

    // 4. 初始化 ADC 电压监测
    adc_monitor_init();

    // 5. 启动 WiFi STA 与 OTA Web 服务器
    start_web_ota();
}
