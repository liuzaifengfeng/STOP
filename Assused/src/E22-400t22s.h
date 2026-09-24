#ifndef E22_400T22S_H
#define E22_400T22S_H

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E22-400T22S 与 ESP32-C6-MINI-1 的网表连接。 */
#define E22_UART_PORT           UART_NUM_1
#define E22_UART_BAUD_RATE      9600
#define E22_PIN_NRST            GPIO_NUM_1
#define E22_PIN_M0              GPIO_NUM_2
#define E22_PIN_M1              GPIO_NUM_3
#define E22_PIN_UART_TX         GPIO_NUM_4 /* ESP32 TX -> E22 RXD */
#define E22_PIN_UART_RX         GPIO_NUM_5 /* ESP32 RX <- E22 TXD */
#define E22_PIN_AUX             GPIO_NUM_14

/* 单个 E22 无线分包最多 240 字节，协议开销为 5 字节。 */
#define E22_FRAME_MAGIC_0       0xE2U
#define E22_FRAME_MAGIC_1       0x22U
#define E22_FRAME_OVERHEAD      5U
#define E22_MAX_PAYLOAD_LEN     235U

typedef enum {
    E22_MODE_UNINIT = 0,
    E22_MODE_TRANSMIT,
    E22_MODE_WOR,
    E22_MODE_CONFIG,
    E22_MODE_SLEEP,
} e22_mode_t;

typedef struct {
    uint8_t data[E22_MAX_PAYLOAD_LEN];
    uint8_t len;
} e22_rx_msg_t;

/**
 * 初始化 GPIO、UART、模块参数以及后台接收任务。
 *
 * 模块参数不匹配时才写入非易失寄存器，写入后会再次读回校验。
 */
esp_err_t e22_init(void);

/**
 * 发送一个带长度和 CRC16 的应用数据帧。
 *
 * @param data          载荷，长度必须为 1..E22_MAX_PAYLOAD_LEN。
 * @param len           载荷长度。
 * @param timeout_ticks 等待互斥锁、AUX 及 UART 完成的总超时。
 */
esp_err_t e22_send(const uint8_t *data, size_t len, TickType_t timeout_ticks);

/** 从内部接收队列获取一个通过 CRC 校验的完整数据帧。 */
esp_err_t e22_receive(e22_rx_msg_t *msg, TickType_t timeout_ticks);

/**
 * 令模块进入深度休眠。sleep_time_ms 为 0 时保持休眠，非 0 时阻塞等待
 * 指定时间并自动唤醒。
 */
esp_err_t e22_sleep(uint32_t sleep_time_ms);

/** 将模块从深度休眠切换回透明传输模式。 */
esp_err_t e22_wakeup(void);

/** 返回驱动记录的当前模块模式。 */
e22_mode_t e22_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* E22_400T22S_H */
