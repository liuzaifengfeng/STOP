#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "ble_device_service.h"
#include "ina226_monitor.h"
#include "radio_transport.h"
#include "ota_service.h"
#include "receiver_safety.h"
#include "vin_monitor.h"
#include "status_led.h"

#define MCU_ENABLE GPIO_NUM_21

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "Receiver Beta boot: output locked OFF");
    ESP_LOGI(TAG, "============================================================");

    /* 第一条硬件操作：拉低输出许可。网表另有 100k 下拉作上电保护。 */
    gpio_set_level(MCU_ENABLE, 0);
    gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << MCU_ENABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
    ESP_LOGI(TAG, "GPIO21 output permission: OFF (low); local key GPIO6: active low");
    ESP_LOGI(TAG, "Pins: E22 M0=2 M1=3 RX=4 TX=5 AUX=14; INA226 SDA=22 SCL=19; VIN ADC=0; LED=20");

    // 初始化 NVS（BLE PHY 和配置依赖 NVS）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS: ready");

    // 根据 Beta 实际网表初始化 INA226；失败时仍保持断电并开放 BLE 诊断。
    esp_err_t ina_err = ina226_monitor_init();
    if (ina_err != ESP_OK) {
        ESP_LOGE(TAG, "INA226 failed: %s; output remains OFF", esp_err_to_name(ina_err));
    } else {
        ina226_sample_t sample;
        esp_err_t sample_err = ina226_monitor_read(&sample);
        if (sample_err == ESP_OK) {
            ESP_LOGI(TAG, "INA226 initial: VOUT=%ld mV, current=%ld mA",
                     (long)sample.bus_mv, (long)sample.current_ma);
        } else {
            ESP_LOGW(TAG, "INA226 initial sample failed: %s",
                     esp_err_to_name(sample_err));
        }
    }
#if VIN_MONITOR_ENABLED
    esp_err_t vin_err = vin_monitor_init();
    if (vin_err != ESP_OK) {
        ESP_LOGE(TAG, "VIN ADC failed: %s; output remains OFF", esp_err_to_name(vin_err));
    } else {
        int32_t vin_mv = 0;
        esp_err_t read_err = vin_monitor_read_mv(&vin_mv);
        if (read_err == ESP_OK) {
            ESP_LOGI(TAG, "VIN ADC initial: %ld mV (R10=100k, R11=10k)",
                     (long)vin_mv);
        } else {
            ESP_LOGW(TAG, "VIN ADC initial sample failed: %s",
                     esp_err_to_name(read_err));
        }
    }

#else
    esp_err_t vin_err = ESP_ERR_NOT_SUPPORTED;
    ESP_LOGW(TAG, "VIN ADC and input voltage protection temporarily DISABLED");
#endif

    // 按 NVS 配置启动无线链路；ESP-NOW 模式不依赖 E22 模块。
    esp_err_t radio_err = radio_transport_init();
    bool radio_ready = radio_err == ESP_OK;
    if (!radio_ready) {
        ESP_LOGE(TAG,
                 "Radio initialization failed: %s; keeping system and BLE online "
                 "for diagnostics",
                 esp_err_to_name(radio_err));
    }

    ESP_ERROR_CHECK(receiver_safety_start(radio_ready));
    if (ina_err != ESP_OK || (VIN_MONITOR_ENABLED && vin_err != ESP_OK) || !radio_ready) {
        ESP_LOGE(TAG, "Startup safety fault: INA226=%s, VIN=%s, radio=%s",
                 esp_err_to_name(ina_err), esp_err_to_name(vin_err), radio_ready ? "OK" : "FAIL");
        receiver_safety_hardware_fault();
    }
    esp_err_t led_err = status_led_start();
    if (led_err != ESP_OK) {
        ESP_LOGW(TAG, "WS2812 unavailable: %s", esp_err_to_name(led_err));
    }

    // BLE 管理服务沿用控制端 GATT UUID 与 STOP 帧格式。
    ESP_ERROR_CHECK(ble_device_service_start(radio_ready));
    ESP_LOGI(TAG, "Startup complete: radio=%s, INA226=%s, VIN ADC=%s, LED=%s; output=%s",
             radio_ready ? "OK" : "FAIL", ina_err == ESP_OK ? "OK" : "FAIL",
             VIN_MONITOR_ENABLED ? (vin_err == ESP_OK ? "OK" : "FAIL") : "DISABLED", led_err == ESP_OK ? "OK" : "FAIL",
             "OFF until paired controller releases E-stop and 3 safe heartbeats arrive");
    ESP_LOGI(TAG, "Serial status repeats every 10 s; hold local key 8 s to pair, 3 s to enable; temporary BLE debug interface also available");

    // OTA 新镜像仅在 INA226 与当前无线链路均正常时确认。
    if (radio_ready && ina_err == ESP_OK) {
        esp_err_t confirm_err = esp_ota_mark_app_valid_cancel_rollback();
        if (confirm_err == ESP_OK || confirm_err == ESP_ERR_NOT_SUPPORTED) {
            ota_service_mark_running_image_confirmed();
        } else {
            ESP_LOGW(TAG, "OTA image confirmation skipped: %s",
                     esp_err_to_name(confirm_err));
        }
    }
}
