#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

// Test GPIO (change to match your board's actual LED pin)
#define BLINK_GPIO GPIO_NUM_22 // GPIO_NUM_22 is the BUZZER pin 

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
    int led_state = 1;

    // Toggle LED state
    led_state = 0;
    gpio_set_level(BLINK_GPIO, led_state);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_state = 1;
    gpio_set_level(BLINK_GPIO, led_state);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_state = 0;
    gpio_set_level(BLINK_GPIO, led_state);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_state = 1;
    gpio_set_level(BLINK_GPIO, led_state);
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {

        // Print counter every second — if serial output is continuous and incrementing, no crash loop is occurring
        ESP_LOGI(TAG, "System running normally... counter: %lu, LED: %d", ++boot_count, led_state);

        if(boot_count % 60 == 0){
            led_state = 0;
            gpio_set_level(BLINK_GPIO, led_state);
            vTaskDelay(pdMS_TO_TICKS(200));
            led_state = 1;
            gpio_set_level(BLINK_GPIO, led_state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}