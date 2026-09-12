#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "ble_device_service.h"
#include "adc_monitor.h"
#include "E22-400t22s.h"

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
static void __attribute__((unused)) e22_rx_demo_task(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "E22 RX demo task started");

    while (1) {
        e22_rx_msg_t msg;
        // 阻塞等待接收数据 (1 秒超时)
        if (e22_receive(&msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
            ESP_LOGI(TAG, "E22 RX [%u bytes]: %.*s", msg.len, msg.len, msg.data);
        }
    }
}

// LoRa 发送演示任务 (定时广播心跳)
static void __attribute__((unused)) e22_tx_demo_task(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "E22 TX demo task started");
    uint32_t seq = 0;

    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        uint8_t data[E22_MAX_PAYLOAD_LEN];
        int len = snprintf((char *)data, sizeof(data), "HEARTBEAT seq=%lu", ++seq);

        if (len > 0 && e22_send(data, (size_t)len, pdMS_TO_TICKS(1000)) == ESP_OK) {
            ESP_LOGI(TAG, "E22 TX accepted: %s", data);
        } else {
            ESP_LOGW(TAG, "E22 TX failed");
        }

        // 每 30 秒广播一次
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C6 boot successful! BLE maintenance transport enabled.");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化 NVS（BLE PHY、后续配对信息和设备参数依赖 NVS）
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

    // 5. 初始化 E22-400T22S。模块故障不应拖垮主系统和 BLE 诊断。
    esp_err_t e22_err = e22_init();
    bool e22_ready = e22_err == ESP_OK;
    if (!e22_ready) {
        ESP_LOGE(TAG,
                 "E22 initialization failed: %s; keeping system and BLE online "
                 "for diagnostics",
                 esp_err_to_name(e22_err));
    }

    // 6. BLE 管理服务：设备信息、状态、参数、E22 诊断和 OTA。
    ESP_ERROR_CHECK(ble_device_service_start(e22_ready));

    // OTA 新镜像只有在关键服务均启动后才确认；E22 自检失败时保留回滚机会。
    if (e22_ready) {
        esp_err_t confirm_err = esp_ota_mark_app_valid_cancel_rollback();
        if (confirm_err != ESP_OK && confirm_err != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "OTA image confirmation skipped: %s",
                     esp_err_to_name(confirm_err));
        }
    }
}
