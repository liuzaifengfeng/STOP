#ifndef E22_400M22S_H
#define E22_400M22S_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 硬件引脚定义 (原理图网表映射) ==================== */
/* ----- SPI (ESP32-C6 FSPI / SPI2) ----- */
#define E22_SPI_HOST        SPI2_HOST
#define E22_PIN_MOSI        GPIO_NUM_20   // U2.17 → U1.26 (FSPID)
#define E22_PIN_MISO        GPIO_NUM_21   // U2.16 → U1.27 (FSPIQ)
#define E22_PIN_SCK         GPIO_NUM_19   // U2.18 → U1.25 (FSPICLK)
#define E22_PIN_NSS         GPIO_NUM_18   // U2.19 → U1.24 (FSPICS0)

/* ----- 控制引脚 ----- */
#define E22_PIN_BUSY        GPIO_NUM_4    // U2.14 → U1.9
#define E22_PIN_DIO1        GPIO_NUM_5   // U2.13 → U1.10
#define E22_PIN_NRST        GPIO_NUM_3   // U2.15 → U1.15
#define E22_PIN_RXEN        GPIO_NUM_1   // U2.6  → U1.13
#define E22_PIN_TXEN        GPIO_NUM_2    // U2.7  → U1.5

/* ==================== SX1268 无线参数默认值 ==================== */
#define E22_RF_FREQ_HZ      433000000UL   // 433 MHz (410~493 MHz 范围内)
#define E22_TX_POWER_DBM    22            // 最大 22 dBm
#define E22_RX_BW_KHZ       125           // 带宽 125 kHz
#define E22_LORA_SF         9             // 扩频因子 9
#define E22_LORA_CR         1             // 编码率 4/7 (1=4/5,2=4/6,3=4/7,4=4/8)
#define E22_LORA_PREAMBLE   8             // 前导码长度
#define E22_PACKET_MAX_LEN  255           // 最大数据包长度

/* ==================== SX1268 操作码 ==================== */
#define SX1268_OP_WRITE_REGISTER        0x0D
#define SX1268_OP_READ_REGISTER         0x1D
#define SX1268_OP_WRITE_BUFFER          0x0E
#define SX1268_OP_READ_BUFFER           0x1E
#define SX1268_OP_GET_STATUS            0xC0
#define SX1268_OP_SET_SLEEP             0x84
#define SX1268_OP_SET_STANDBY           0x80
#define SX1268_OP_SET_FS                0xC1
#define SX1268_OP_SET_TX                0x83
#define SX1268_OP_SET_RX                0x82
#define SX1268_OP_SET_CAD               0xC5
#define SX1268_OP_SET_PACKET_TYPE       0x8A
#define SX1268_OP_SET_RF_FREQUENCY      0x86
#define SX1268_OP_SET_TX_PARAMS         0x8E
#define SX1268_OP_SET_MODULATION_PARAMS 0x8B
#define SX1268_OP_SET_PACKET_PARAMS     0x8C
#define SX1268_OP_SET_BUFFER_BASE_ADDR  0x8F
#define SX1268_OP_SET_DIO_IRQ_PARAMS    0x08
#define SX1268_OP_GET_IRQ_STATUS        0x12
#define SX1268_OP_CLEAR_IRQ_STATUS      0x02
#define SX1268_OP_GET_PACKET_STATUS     0x14
#define SX1268_OP_GET_RSSI_INST         0x15
#define SX1268_OP_CALIBRATE             0x89
#define SX1268_OP_SET_RX_TX_FALLBACK    0x93

/* ==================== SX1268 命令参数 ==================== */
/* SetSleep 参数 */
#define SX1268_SLEEP_COLD_START     0x00
#define SX1268_SLEEP_WARM_START     0x04

/* SetStandby 参数 */
#define SX1268_STANDBY_RC           0x00
#define SX1268_STANDBY_XOSC         0x01

/* SetPacketType 参数 */
#define SX1268_PACKET_TYPE_GFSK     0x00
#define SX1268_PACKET_TYPE_LORA     0x01

/* SetRx / SetTx 超时 (单位: 15.625 us) */
#define SX1268_TIMEOUT_NONE         0x000000

/* SetRx 模式 */
#define SX1268_RX_SINGLE            0x000000
#define SX1268_RX_CONTINUOUS        0xFFFFFF

/* IRQ 状态位 */
#define SX1268_IRQ_TX_DONE          0x0001
#define SX1268_IRQ_RX_DONE          0x0002
#define SX1268_IRQ_PREAMBLE_DET     0x0004
#define SX1268_IRQ_SYNC_VALID       0x0008
#define SX1268_IRQ_HEADER_VALID     0x0010
#define SX1268_IRQ_HEADER_ERR       0x0020
#define SX1268_IRQ_CRC_ERR          0x0040
#define SX1268_IRQ_CAD_DONE         0x0080
#define SX1268_IRQ_CAD_DETECTED     0x0100
#define SX1268_IRQ_TIMEOUT          0x0200
#define SX1268_IRQ_ALL              0x03FF

/* ==================== 状态枚举 ==================== */
typedef enum {
    E22_MODE_UNINIT = 0,  // 未初始化
    E22_MODE_SLEEP,       // 睡眠
    E22_MODE_STANDBY,     // 待机
    E22_MODE_TX,          // 发送
    E22_MODE_RX,          // 接收
    E22_MODE_CAD          // 信道检测
} e22_mode_t;

/* ==================== 消息队列数据结构 ==================== */

// LoRa 发送请求 (通过队列投递 → LoRa 任务发送)
typedef struct {
    uint8_t data[E22_PACKET_MAX_LEN];
    uint8_t len;
} lora_tx_msg_t;

// LoRa 接收数据 (由 LoRa 任务发布 → 其他任务消费)
typedef struct {
    uint8_t data[E22_PACKET_MAX_LEN];
    uint8_t len;
    int16_t rssi;       // RSSI (dBm)
    int8_t  snr;        // 信噪比 (dB)
} lora_rx_msg_t;

/* ==================== 对外接口 ==================== */

/**
 * 初始化 E22-400M22S 模块:
 *   - 配置 SPI2 外设
 *   - 初始化控制引脚 (NSS, BUSY, DIO1, NRST, RXEN, TXEN)
 *   - 复位模块并配置 SX1268 寄存器
 *   - 创建 LoRa FreeRTOS 任务和消息队列
 */
void e22_init(void);

/**
 * 获取 LoRa 发送队列句柄 (其他任务可向该队列发送 lora_tx_msg_t)
 * @return 发送队列句柄, 若未初始化则返回 NULL
 */
QueueHandle_t e22_get_tx_queue(void);

/**
 * 获取 LoRa 接收队列句柄 (其他任务可从该队列读取 lora_rx_msg_t)
 * @return 接收队列句柄, 若未初始化则返回 NULL
 */
QueueHandle_t e22_get_rx_queue(void);

/**
 * 获取当前模块工作模式
 * @return 当前模式
 */
e22_mode_t e22_get_mode(void);

/**
 * 向发送队列投递一条消息 (非阻塞)
 * @param msg 指向发送消息结构体
 * @return true=成功入队, false=队列满或参数无效
 */
bool e22_send_msg(const lora_tx_msg_t *msg);

/**
 * 从接收队列获取一条消息 (非阻塞)
 * @param msg 指向接收消息结构体 (输出参数)
 * @return true=成功出队, false=队列空或参数无效
 */
bool e22_recv_msg(lora_rx_msg_t *msg);

/**
 * 向发送队列投递一条消息 (阻塞)
 * @param msg           指向发送消息结构体
 * @param timeout_ticks FreeRTOS 滴答超时 (如 pdMS_TO_TICKS(200))
 * @return true=成功入队, false=超时或参数无效
 */
bool e22_send_msg_blocking(const lora_tx_msg_t *msg, TickType_t timeout_ticks);

/**
 * 从接收队列获取一条消息 (阻塞)
 * @param msg           指向接收消息结构体 (输出参数)
 * @param timeout_ticks FreeRTOS 滴答超时
 * @return true=成功出队, false=超时或参数无效
 */
bool e22_recv_msg_blocking(lora_rx_msg_t *msg, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif // E22_400M22S_H
