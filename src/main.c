#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "web_ota.h"
#include "adc_monitor.h"
#include "E22-400m22s.h"

#define BLINK_GPIO GPIO_NUM_22 // GPIO_NUM_22 is the BUZZER pin

static const char *TAG = "main";

//蜂鸣器任务（物理心跳）
static void blink_task(void *pvParameter)
{
    uint32_t boot_count = 0;
    int buzzer_state = 1;

    // 初始状态翻转测试
    for (int i = 0; i < 4; i++) {
        buzzer_state = !buzzer_state;
        gpio_set_level(BLINK_GPIO, buzzer_state);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    while (1) {
        ESP_LOGI(TAG, "System running normally... counter: %lu", ++boot_count);

        if (boot_count % 60 == 0) {
            buzzer_state = 0;
            gpio_set_level(BLINK_GPIO, buzzer_state);
            vTaskDelay(pdMS_TO_TICKS(200));
            buzzer_state = 1;
            gpio_set_level(BLINK_GPIO, buzzer_state);
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// LoRa 接收演示任务 (从 E22 接收队列读取数据)
static void lora_rx_demo_task(void *pvParameter)
{
    ESP_LOGI(TAG, "LoRa RX demo task started");
    QueueHandle_t rxq = e22_get_rx_queue();

    while (1) {
        lora_rx_msg_t msg;
        // 阻塞等待接收数据 (1 秒超时)
        if (pdTRUE == xQueueReceive(rxq, &msg, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "LoRa RX [%d bytes] RSSI=%d SNR=%d: %.*s",
                     msg.len, msg.rssi, msg.snr,
                     msg.len, msg.data);
        }
    }
}

// LoRa 发送演示任务 (定时广播心跳)
static void lora_tx_demo_task(void *pvParameter)
{
    ESP_LOGI(TAG, "LoRa TX demo task started");
    QueueHandle_t txq = e22_get_tx_queue();
    uint32_t seq = 0;

    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        lora_tx_msg_t msg;
        snprintf((char *)msg.data, sizeof(msg.data),
                 "HEARTBEAT seq=%lu", ++seq);
        msg.len = strlen((char *)msg.data);

        if (pdTRUE == xQueueSend(txq, &msg, pdMS_TO_TICKS(100))) {
            ESP_LOGI(TAG, "LoRa TX queued: %s", msg.data);
        } else {
            ESP_LOGW(TAG, "LoRa TX queue full");
        }

        // 每 30 秒广播一次
        vTaskDelay(pdMS_TO_TICKS(30000));
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

    // 6. 初始化 E22-400M22S LoRa 模块
    e22_init();

    // 7. 创建 LoRa 收发演示任务 (可根据需要启用)
    //xTaskCreate(lora_rx_demo_task, "lora_rx_demo", 4096, NULL, 5, NULL);
    //xTaskCreate(lora_tx_demo_task, "lora_tx_demo", 4096, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(20000));
    for (int i = 0; i < 1000; i++){
        e22_sleep(0);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_light_sleep(1000 * 60 * 60 * 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        e22_wakeup();
        ESP_LOGI(TAG, "Woke up from light sleep %d", i);
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
