#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

// Test GPIO (change to match your board's actual LED pin)
// If no LED is connected, serial log output is unaffected
#define BLINK_GPIO GPIO_NUM_22

static const char *TAG = "main";

void app_main(void) 
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C6 boot successful! 4MB Flash firmware running.");
    ESP_LOGI(TAG, "==================================================");

    // Initialize test GPIO
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint32_t boot_count = 0;
    int led_state = 0;

    while (1) {
        // Toggle LED state
        led_state = !led_state;
        gpio_set_level(BLINK_GPIO, led_state);

        // Print counter every second — if serial output is continuous and incrementing, no crash loop is occurring
        ESP_LOGI(TAG, "System running normally... counter: %lu, LED: %d", ++boot_count, led_state);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}