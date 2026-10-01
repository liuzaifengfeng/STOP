#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdbool.h>
#include "esp_err.h"

/* 控制端 WS2812：GPIO8 数据、GPIO9 低电平开启灯电源。 */
esp_err_t status_led_start(bool radio_ready);

#endif
