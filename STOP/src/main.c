#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "ble_device_service.h"
#include "adc_monitor.h"
#include "radio_transport.h"
#include "ota_service.h"
#include "controller_safety.h"
#include "status_led.h"

#define BLINK_GPIO GPIO_NUM_22 // GPIO_NUM_22 is the BUZZER pin

static const char *TAG = "main";
static TaskHandle_t s_buzzer_task;
static bool s_radio_ready;

/* 睡眠期间保持外围硬件关闭，无定时唤醒；充电后重新上电/复位。 */
static void hold_output(gpio_num_t pin, int level)
{
    gpio_set_level(pin, level);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
    ESP_ERROR_CHECK(gpio_hold_en(pin));
}

static void low_battery_sleep(bool boot)
{
    ESP_LOGW(TAG, "Battery <= %d mV: %s, entering deep sleep",
             BATTERY_SHUTDOWN_MV, boot ? "boot denied" : "shutdown latched");
    if (!boot && s_radio_ready) controller_safety_shutdown();
    if (s_buzzer_task) vTaskSuspend(s_buzzer_task);
    /* 低电压只短鸣一次，避免蜂鸣器持续耗电。当前蜂鸣器低电平有效。 */
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(BLINK_GPIO, 1);
    if (radio_transport_e22_ready()) {
        esp_err_t err = e22_sleep(0);
        if (err != ESP_OK)
            ESP_LOGW(TAG, "E22 sleep failed: %s; forcing sleep pins", esp_err_to_name(err));
    }
    /* 即使无线初始化失败，也让 M0/M1 进入休眠组合。 */
    hold_output(E22_PIN_M0, 1);
    hold_output(E22_PIN_M1, 1);
    hold_output(GPIO_NUM_9, 1); /* LED 高边开关关闭 */
    hold_output(BLINK_GPIO, 1);
    /* ESP32-C6 的 gpio_hold_en 本身即可跨深睡眠保持单个引脚。 */
    ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
    esp_deep_sleep_start();
}

static bool battery_is_low(void)
{
    BatteryInfo info;
    return get_battery_info(&info) &&
           info.voltage_v <= BATTERY_SHUTDOWN_MV / 1000.0f;
}

static void battery_protection_task(void *arg)
{
    (void)arg;
    while (true) {
        if (battery_is_low()) low_battery_sleep(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

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

void app_main(void)
{
    gpio_hold_dis(BLINK_GPIO);
    gpio_hold_dis(E22_PIN_M0);
    gpio_hold_dis(E22_PIN_M1);
    gpio_hold_dis(GPIO_NUM_9);
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "ESP32-C6 boot successful!  BLE maintenance transport enabled.");
    ESP_LOGI(TAG, "============================================================");

    // 1. 先检查电池；低压时不启动无线、BLE 和正常业务任务。
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_level(BLINK_GPIO, 1);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    adc_monitor_init();
    if (battery_is_low()) low_battery_sleep(true);

    // 2. 初始化 NVS（BLE PHY、后续配对信息和设备参数依赖 NVS）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 3. 创建业务任务
    ESP_ERROR_CHECK(xTaskCreate(blink_task, "blink_task", 4096, NULL, 5, &s_buzzer_task)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    // 5. 按 NVS 配置启动无线链路；故障时继续开放 BLE 诊断与配置。
    esp_err_t radio_err = radio_transport_init();
    bool radio_ready = radio_err == ESP_OK;
    if (!radio_ready) {
        ESP_LOGE(TAG,
                 "Radio initialization failed: %s; keeping system and BLE online "
                 "for diagnostics",
                 esp_err_to_name(radio_err));
    }
    s_radio_ready = radio_ready;
    /* 无线初始化期间可能继续掉压，发出第一帧安全心跳前再次检查。 */
    if (battery_is_low()) low_battery_sleep(true);
    if (radio_ready) {
        ESP_ERROR_CHECK(controller_safety_start(true));
    }
    ESP_ERROR_CHECK(xTaskCreate(battery_protection_task, "battery_protect", 4096,
                               NULL, 9, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    esp_err_t led_err = status_led_start(radio_ready);
    if (led_err != ESP_OK)
        ESP_LOGW(TAG, "WS2812 unavailable: %s", esp_err_to_name(led_err));

    // 6. BLE 管理服务：设备信息、状态、参数、E22 诊断和 OTA。
    ESP_ERROR_CHECK(ble_device_service_start(radio_ready));

    // OTA 新镜像只有在当前无线链路及关键服务启动后才确认。
    if (radio_ready) {
        esp_err_t confirm_err = esp_ota_mark_app_valid_cancel_rollback();
        if (confirm_err == ESP_OK || confirm_err == ESP_ERR_NOT_SUPPORTED) {
            ota_service_mark_running_image_confirmed();
        } else {
            ESP_LOGW(TAG, "OTA image confirmation skipped: %s",
                     esp_err_to_name(confirm_err));
        }
    }
}
