#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "E22-400m22s.h"

static const char *TAG = "E22";

/* ==================== 模块内部变量 ==================== */
static spi_device_handle_t spi_dev;
static e22_mode_t g_mode = E22_MODE_UNINIT;

// 消息队列句柄
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;

// 任务句柄
static TaskHandle_t lora_task_handle = NULL;

// RXEN / TXEN 由硬件控; 如需纯 LoRa 收发则分别为高/低
// 当前设计: 默认进入连续接收模式 (RXEN=1, TXEN=0), 发送时切换

/* ==================== 底层辅助函数 ==================== */

// 等待 BUSY 引脚释放 (低电平 = 就绪), 带超时
static esp_err_t wait_busy(uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (gpio_get_level(E22_PIN_BUSY)) {
        if (elapsed++ >= timeout_ms) {
            ESP_LOGE(TAG, "BUSY timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

// 控制 RF 开关: TX 模式
static inline void rf_switch_tx(void) {
    gpio_set_level(E22_PIN_RXEN, 0);
    gpio_set_level(E22_PIN_TXEN, 1);
}

// 控制 RF 开关: RX 模式
static inline void rf_switch_rx(void) {
    gpio_set_level(E22_PIN_TXEN, 0);
    gpio_set_level(E22_PIN_RXEN, 1);
}

// 控制 RF 开关: 空闲
static inline void rf_switch_idle(void) {
    gpio_set_level(E22_PIN_RXEN, 0);
    gpio_set_level(E22_PIN_TXEN, 0);
}

/* ==================== SX1268 SPI 命令 ==================== */

// 发送一条 SPI 命令 (无参数或带 MOSI 数据), 不读取 MISO
static void sx1268_write_cmd(uint8_t opcode, const uint8_t *data, uint8_t len) {
    wait_busy(1000);
    uint8_t tx_buf[256];
    tx_buf[0] = opcode;
    if (len > 0 && data != NULL) {
        memcpy(tx_buf + 1, data, len);
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t)); // 清零初始化结构体
    t.length = (1 + len) * 8;

    if (1 + len <= 4) {
        t.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.tx_data, tx_buf, 1 + len); // 正确复制数据到 tx_data 数组
    } else {
        t.tx_buffer = tx_buf;               // 使用外部 buffer 指针
    }
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));
}

// 写命令 + 读回数据
static void sx1268_read_cmd(uint8_t opcode, uint8_t *rx_data, uint16_t rx_len) {
    wait_busy(1000);
    uint8_t tx_buf[256] = {0};
    uint8_t rx_buf[256] = {0};
    tx_buf[0] = opcode;

    // SX1268 标准命令读：Opcode(1B) -> 芯片返回 Status(1B) + 数据(rx_len)
    // 只有读寄存器 (0x1D) 机制才需要额外的 Dummy Byte
    uint16_t total_len = 1 + rx_len;

    spi_transaction_t t = {
        .length    = total_len * 8,
        .rxlength  = total_len * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));

    // 偏移 1 字节提取数据（rx_buf[0] 是发送 opcode 时 MISO 上的 status）
    if (rx_len > 0 && rx_data != NULL) {
        memcpy(rx_data, rx_buf + 1, rx_len);
    }
}

// 写命令 + 同时写参数 + 读回数据
static void __attribute__((unused)) sx1268_write_read(uint8_t opcode, const uint8_t *tx_data, uint8_t tx_len,
                              uint8_t *rx_data, uint16_t rx_len) {
    wait_busy(1000);
    // SPI 模式下: 先发 opcode, 再发 tx_data, 再读 rx_len 字节
    // 注意: 需关闭 USE_TXDATA/USE_RXDATA, 使用独立 buffer
    uint8_t tx_buf[tx_len + 1];
    tx_buf[0] = opcode;
    if (tx_len > 0) memcpy(tx_buf + 1, tx_data, tx_len);

    uint8_t rx_buf[tx_len + rx_len + 1];
    spi_transaction_t t = {
        .length    = 8 + tx_len * 8 + rx_len * 8,
        .rxlength  = rx_len * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));
    // 从 rx_buf 中提取读取的数据 (data 从 tx_len+1 之后开始)
    if (rx_len > 0) {
        memcpy(rx_data, rx_buf + tx_len + 1, rx_len);
    }
}

// 快速 SPI 全双工交换 (opcode + tx_data, 返回 opcode 状态字 + rx 数据)
// 对于 SX1268: 发送 opcode(1B) + 参数 → 立即读取 status(1B) + data
static uint8_t __attribute__((unused)) sx1268_spi_transfer(uint8_t opcode, const uint8_t *tx_data, uint8_t tx_len,
                                    uint8_t *rx_data, uint16_t rx_len) {
    wait_busy(1000);
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    tx_buf[0] = opcode;
    if (tx_len > 0) memcpy(tx_buf + 1, tx_data, tx_len);

    uint16_t total_len = 1 + tx_len + (rx_len > 0 ? (rx_len + 1) : 0);
    spi_transaction_t t = {
        .length    = total_len * 8,
        .rxlength  = total_len * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));

    // SX1268 在命令字后返回 1 字节状态, 然后是数据
    uint8_t status = rx_buf[1]; // 索引 0=opcode, 1=status
    if (rx_len > 0) {
        memcpy(rx_data, rx_buf + 2, rx_len);
    }
    return status;
}

/* ==================== SX1268 上层操作函数 ==================== */

uint8_t sx1268_get_status(void) {
    uint8_t status;
    sx1268_read_cmd(SX1268_OP_GET_STATUS, &status, 1);
    return status;
}

static void __attribute__((unused)) sx1268_set_sleep(uint8_t config) {
    uint8_t data[] = { config };
    sx1268_write_cmd(SX1268_OP_SET_SLEEP, data, 1);
}

static void sx1268_set_standby(uint8_t config) {
    uint8_t data[] = { config };
    sx1268_write_cmd(SX1268_OP_SET_STANDBY, data, 1);
}

static void sx1268_set_packet_type(uint8_t packet_type) {
    uint8_t data[] = { packet_type };
    sx1268_write_cmd(SX1268_OP_SET_PACKET_TYPE, data, 1);
}

static void sx1268_set_rf_frequency(uint32_t freq_hz) {
    // RF 频率计算: RF_Freq = INT(FREQ_Hz * 2^25 / 32e6)
    uint64_t reg_val = ((uint64_t)freq_hz << 25) / 32000000UL;
    uint8_t data[4] = {
        (uint8_t)(reg_val >> 24),
        (uint8_t)(reg_val >> 16),
        (uint8_t)(reg_val >> 8),
        (uint8_t)(reg_val),
    };
    sx1268_write_cmd(SX1268_OP_SET_RF_FREQUENCY, data, 4);
}

static void sx1268_set_tx_params(uint8_t power, uint8_t ramp_time) {
    // power = 22 dBm → SX1268 值 = 0x16 (22)
    uint8_t data[2] = { power, ramp_time };
    sx1268_write_cmd(SX1268_OP_SET_TX_PARAMS, data, 2);
}

static void sx1268_set_modulation_params(uint8_t sf, uint8_t bw, uint8_t cr) {
    uint8_t data[4] = { sf, bw, cr, 0x00 };
    sx1268_write_cmd(SX1268_OP_SET_MODULATION_PARAMS, data, 4);
}

static void sx1268_set_packet_params(uint16_t preamble, uint8_t header_type,
                                      uint8_t payload_len, uint8_t crc_type,
                                      uint8_t invert_iq) {
    uint8_t data[6] = {
        (uint8_t)(preamble >> 8),
        (uint8_t)(preamble),
        header_type,
        payload_len,
        crc_type,
        invert_iq,
    };
    sx1268_write_cmd(SX1268_OP_SET_PACKET_PARAMS, data, 6);
}

static void sx1268_set_buffer_base_addr(uint8_t tx_base, uint8_t rx_base) {
    uint8_t data[2] = { tx_base, rx_base };
    sx1268_write_cmd(SX1268_OP_SET_BUFFER_BASE_ADDR, data, 2);
}

static void sx1268_set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask,
                                       uint16_t dio2_mask, uint16_t dio3_mask) {
    uint8_t data[8] = {
        (uint8_t)(irq_mask >> 8), (uint8_t)(irq_mask),
        (uint8_t)(dio1_mask >> 8), (uint8_t)(dio1_mask),
        (uint8_t)(dio2_mask >> 8), (uint8_t)(dio2_mask),
        (uint8_t)(dio3_mask >> 8), (uint8_t)(dio3_mask),
    };
    sx1268_write_cmd(SX1268_OP_SET_DIO_IRQ_PARAMS, data, 8);
}

static uint16_t sx1268_get_irq_status(void) {
    uint8_t data[2];
    sx1268_read_cmd(SX1268_OP_GET_IRQ_STATUS, data, 2);
    return ((uint16_t)data[0] << 8) | data[1];
}

static void sx1268_clear_irq_status(uint16_t mask) {
    uint8_t data[2] = { (uint8_t)(mask >> 8), (uint8_t)(mask) };
    sx1268_write_cmd(SX1268_OP_CLEAR_IRQ_STATUS, data, 2);
}

static void sx1268_set_tx(uint32_t timeout) {
    uint8_t data[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout),
    };
    sx1268_write_cmd(SX1268_OP_SET_TX, data, 3);
}

static void sx1268_set_rx(uint32_t timeout) {
    uint8_t data[3] = {
        (uint8_t)(timeout >> 16),
        (uint8_t)(timeout >> 8),
        (uint8_t)(timeout),
    };
    sx1268_write_cmd(SX1268_OP_SET_RX, data, 3);
}

// 写数据到发送缓冲区
static void sx1268_write_buffer(uint8_t offset, const uint8_t *data, uint8_t len) {
    wait_busy(1000);
    uint8_t tx_buf[256];
    tx_buf[0] = offset;
    memcpy(tx_buf + 1, data, len);

    spi_transaction_t t = {
        .length    = (2 + len) * 8,
        .tx_buffer = tx_buf,
        .cmd       = SX1268_OP_WRITE_BUFFER,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));
}

// 读取接收缓冲区
static void sx1268_read_buffer(uint8_t offset, uint8_t *data, uint8_t len) {
    wait_busy(1000);
    uint8_t tx_byte = offset;
    spi_transaction_t t = {
        .length    = (1 + len + 1) * 8, // 1B offset + len data + 1B status
        .rxlength  = (1 + len + 1) * 8,
        .tx_buffer = &tx_byte,
        .rx_buffer = data,
        .cmd       = SX1268_OP_READ_BUFFER,
    };
    // 简单方式: 分两步
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev, &t));
}

// 读取丢包状态
static void sx1268_get_packet_status(lora_rx_msg_t *msg) {
    uint8_t status_buf[3];
    sx1268_read_cmd(SX1268_OP_GET_PACKET_STATUS, status_buf, 3);
    msg->rssi = -(int16_t)(status_buf[0] / 2);
    msg->snr  = (int8_t)(status_buf[1] / 4);
}

// 校准
static void sx1268_calibrate(void) {
    uint8_t data[] = { 0x7F }; 
    sx1268_write_cmd(SX1268_OP_CALIBRATE, data, 1);
    
    // 关键点：Calibrate 耗时较长，必须强制 wait_busy 或延时 5ms 以上
    vTaskDelay(pdMS_TO_TICKS(10)); 
    wait_busy(1000);
}

// ESP32 FSPI 芯片选择 (通过 GPIO 手动控制 NSS)
static inline void spi_nss_low(void)  { gpio_set_level(E22_PIN_NSS, 0); }
static inline void spi_nss_high(void) { gpio_set_level(E22_PIN_NSS, 1); }

// 预定义 SPI 传输回调 (必需,用于 FSPI 半双工控制 NSS)
// 使用默认回调即可; 配置 cs_ena_posttrans 保证 NSS 时序
static void IRAM_ATTR spi_pre_cb(spi_transaction_t *t) {
    spi_nss_low();
}

static void IRAM_ATTR spi_post_cb(spi_transaction_t *t) {
    spi_nss_high();
}

/* ==================== 模块初始化和配置 ==================== */

// 硬件初始化: SPI2 外设及 GPIO 控制引脚
static void e22_hw_init(void) {
    ESP_LOGI(TAG, "HW init: configuring SPI2 (FSPI) and control pins...");

/* ----- 1. 配置输出引脚 (NSS, NRST, RXEN, TXEN) ----- */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << E22_PIN_NSS)
                      | (1ULL << E22_PIN_NRST)
                      | (1ULL << E22_PIN_RXEN)
                      | (1ULL << E22_PIN_TXEN),
        .mode = GPIO_MODE_OUTPUT,            // 纯输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    // 设置初始状态
    gpio_set_level(E22_PIN_NSS, 1);
    gpio_set_level(E22_PIN_NRST, 1);
    gpio_set_level(E22_PIN_RXEN, 0);
    gpio_set_level(E22_PIN_TXEN, 0);

    /* ----- 2. 配置输入引脚 (BUSY, DIO1) ----- */
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << E22_PIN_BUSY)
                      | (1ULL << E22_PIN_DIO1),
        .mode = GPIO_MODE_INPUT,             // 纯输入模式
        .pull_up_en = GPIO_PULLUP_DISABLE,   // 禁用上拉，建议开启下拉
        .pull_down_en = GPIO_PULLDOWN_ENABLE,// 开启下拉，确保未连线时默认为0
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    /* ----- 3. 配置 SPI2 主机 ----- */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = E22_PIN_MOSI,
        .miso_io_num     = E22_PIN_MISO,
        .sclk_io_num     = E22_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 256,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(E22_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {
        .mode           = 0,       // CPOL=0, CPHA=0
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz (安全速率; SX1268 支持最高 18 MHz)
        .spics_io_num   = -1,      // 手动控制 NSS
        .queue_size     = 7,
        .pre_cb         = spi_pre_cb,
        .post_cb        = spi_post_cb,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(E22_SPI_HOST, &dev_cfg, &spi_dev));

    ESP_LOGI(TAG, "SPI2 (FSPI) initialized: MOSI=%d MISO=%d SCK=%d NSS=%d @1MHz",
             E22_PIN_MOSI, E22_PIN_MISO, E22_PIN_SCK, E22_PIN_NSS);
}

// 模块软复位
static void e22_reset(void) {
    ESP_LOGI(TAG, "Resetting module...");
    gpio_set_level(E22_PIN_NRST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(E22_PIN_NRST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    // 等待 BUSY 释放
    wait_busy(1000);
    ESP_LOGI(TAG, "Module reset complete");
}

// 配置 SX1268 射频参数 (LoRa)
static void e22_configure(void) {
    ESP_LOGI(TAG, "Configuring SX1268 LoRa parameters...");
    // 测试 SPI 通信是否双向正常
    uint8_t status = sx1268_get_status(); // 调用你已有的隐藏函数
    ESP_LOGI(TAG, "SX1268 Check Status: 0x%02X", status);

    // 1. 进入待机模式
    sx1268_set_standby(SX1268_STANDBY_RC);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 2. 设置包类型: LoRa
    sx1268_set_packet_type(SX1268_PACKET_TYPE_LORA);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 3. 设置射频频率 (433 MHz)
    sx1268_set_rf_frequency(E22_RF_FREQ_HZ);
    ESP_LOGI(TAG, "  Frequency: %lu Hz", E22_RF_FREQ_HZ);

    // 4. 设置发射参数: 功率 22 dBm, ramp time = 200 us (0x00)
    sx1268_set_tx_params(0x16, 0x00);
    ESP_LOGI(TAG, "  TX Power: 22 dBm, Ramp: 200us");

    // 5. 设置 LoRa 调制参数
    //    SF=9, BW=125kHz (0x05), CR=4/7 (0x02)
    sx1268_set_modulation_params(E22_LORA_SF, 0x05, E22_LORA_CR);
    ESP_LOGI(TAG, "  LoRa: SF=%d, BW=125kHz, CR=4/%d", E22_LORA_SF, 5 + E22_LORA_CR);

    // 6. 设置包参数
    //    preamble=8, implicit header=0, max_payload=255, CRC=1, IQ normal=0
    sx1268_set_packet_params(E22_LORA_PREAMBLE, 0x00, E22_PACKET_MAX_LEN, 0x01, 0x00);
    ESP_LOGI(TAG, "  Preamble=%d, Payload=%d, CRC=ON", E22_LORA_PREAMBLE, E22_PACKET_MAX_LEN);

    // 7. 设置缓冲基址: TX=0x00, RX=0x00
    sx1268_set_buffer_base_addr(0x00, 0x00);

    // 8. 配置 DIO1 为 TX_DONE | RX_DONE 中断
    sx1268_set_dio_irq_params(SX1268_IRQ_TX_DONE | SX1268_IRQ_RX_DONE,
                               SX1268_IRQ_TX_DONE | SX1268_IRQ_RX_DONE,
                               0x0000, 0x0000);
    // 清除所有 IRQ
    sx1268_clear_irq_status(SX1268_IRQ_ALL);

    // 9. 执行校准
    sx1268_calibrate();
    vTaskDelay(pdMS_TO_TICKS(10));

    // 测试 SPI 通信是否双向正常
    status = sx1268_get_status(); // 调用你已有的隐藏函数
    ESP_LOGI(TAG, "SX1268 Check Status: 0x%02X", status);

    // 10. 进入待机, 准备切换至接收
    sx1268_set_standby(SX1268_STANDBY_RC);
    g_mode = E22_MODE_STANDBY;
    ESP_LOGI(TAG, "SX1268 configuration complete");
}

// 发送数据包 (阻塞式, 由 LoRa 任务调用)
static bool e22_transmit(const uint8_t *data, uint8_t len) {
    // 第 437 行 e22_transmit 内：
    #if (E22_PACKET_MAX_LEN < 255)
        if (len == 0 || len > E22_PACKET_MAX_LEN) {
    #else
        if (len == 0) {
    #endif
        ESP_LOGW(TAG, "Invalid tx length: %d", len);
        return false;
    }

    ESP_LOGI(TAG, "TX: sending %d bytes...", len);

    // 1. 待机
    sx1268_set_standby(SX1268_STANDBY_RC);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 2. 写数据到发送缓冲
    sx1268_write_buffer(0x00, data, len);
    // 修改包参数为实际长度
    sx1268_set_packet_params(E22_LORA_PREAMBLE, 0x00, len, 0x01, 0x00);

    // 3. 切换到 TX RF 开关
    rf_switch_tx();
    g_mode = E22_MODE_TX;

    // 4. 启动发送 (无超时)
    sx1268_set_tx(SX1268_TIMEOUT_NONE);

    // 5. 等待 DIO1 上升沿 (TX_DONE)
    uint32_t wait_cnt = 0;
    while (!gpio_get_level(E22_PIN_DIO1)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++wait_cnt > 5000) { // 5秒超时
            ESP_LOGE(TAG, "TX timeout!");
            sx1268_clear_irq_status(SX1268_IRQ_ALL);
            rf_switch_idle();
            g_mode = E22_MODE_STANDBY;
            return false;
        }
    }

    // 6. 清除 IRQ
    uint16_t irq = sx1268_get_irq_status();
    sx1268_clear_irq_status(SX1268_IRQ_ALL);

    if (irq & SX1268_IRQ_TX_DONE) {
        ESP_LOGI(TAG, "TX done: %d bytes sent", len);
    } else if (irq & SX1268_IRQ_TIMEOUT) {
        ESP_LOGW(TAG, "TX timeout IRQ");
        rf_switch_idle();
        g_mode = E22_MODE_STANDBY;
        return false;
    }

    // 7. 切回收听模式
    rf_switch_rx();
    return true;
}

// 启动连续接收
static void e22_start_rx(void) {
    rf_switch_rx();
    sx1268_set_standby(SX1268_STANDBY_RC);
    vTaskDelay(pdMS_TO_TICKS(1));
    sx1268_set_rx(SX1268_RX_CONTINUOUS);
    g_mode = E22_MODE_RX;
    ESP_LOGI(TAG, "RX: continuous mode started");
}

/* ==================== LoRa FreeRTOS 任务 ==================== */
static void lora_task(void *pvParameter) {
    ESP_LOGI(TAG, "LoRa task started");

    // 1. 硬件及 SPI 初始化
    e22_hw_init();

    // 2. 模块复位
    e22_reset();

    // 3. 配置射频参数
    e22_configure();

    // 4. 进入连续接收模式
    e22_start_rx();

    // ===== 主循环 =====
    while (1) {
        // ---- 检查发送队列 ----
        lora_tx_msg_t tx_msg;
        if (pdTRUE == xQueueReceive(tx_queue, &tx_msg, 0)) {
            // 停止接收
            sx1268_set_standby(SX1268_STANDBY_RC);
            // 发送数据 (阻塞)
            bool ok = e22_transmit(tx_msg.data, tx_msg.len);
            if (!ok) {
            ESP_LOGW(TAG, "TX failed!");
            }
            // 发送后恢复连续接收
            e22_start_rx();
        }

        // ---- 检查 DIO1 (RX_DONE 中断) ----
        if (gpio_get_level(E22_PIN_DIO1)) {
            // 读取中断状态
            uint16_t irq = sx1268_get_irq_status();
            sx1268_clear_irq_status(SX1268_IRQ_ALL);

            if (irq & SX1268_IRQ_RX_DONE) {
                // 读取接收包状态 (RSSI, SNR)
                lora_rx_msg_t rx_msg;
                sx1268_get_packet_status(&rx_msg);

                // 读取实际负载长度: 先读 RxPayloadLength(寄存器 0x015B) 或读 buffer 首字节长度
                // 简化方式: 读取最大长度, 通过 SX1268 的 GetRxBufferStatus 读取实际长度
                uint8_t rx_buf[2];
                sx1268_read_cmd(0x13, rx_buf, 2); // GetRxBufferStatus
                uint8_t payload_len = rx_buf[0]; // RxBufferPointer (已收长度)

                rx_msg.len = payload_len;

                if (payload_len > 0) {
                    // 读接收缓冲区
                    sx1268_read_buffer(0x00, (uint8_t *)&rx_msg.data, payload_len);
                    // 投递到接收队列 (非阻塞, 若队列满则丢弃)
                    if (pdTRUE != xQueueSend(rx_queue, &rx_msg, 0)) {
                        ESP_LOGW(TAG, "RX queue full, dropping packet");
                    } else {
                        ESP_LOGI(TAG, "RX: %d bytes, RSSI=%d dBm, SNR=%d dB",
                                 rx_msg.len, rx_msg.rssi, rx_msg.snr);
                    }
                }
            } else if (irq & SX1268_IRQ_CRC_ERR) {
                ESP_LOGW(TAG, "RX CRC error");
            } else if (irq & SX1268_IRQ_TIMEOUT) {
                ESP_LOGD(TAG, "RX timeout (non-critical)");
            }
            // 连续接收模式下, 收到包后自动回到接收, 无需手动重启
        }

        // 避免空转过热
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ==================== 对外接口 ==================== */

void e22_init(void) {
    if (tx_queue || rx_queue) {
        ESP_LOGW(TAG, "Already initialized");
        return;
    }

    // 创建消息队列
    tx_queue = xQueueCreate(8, sizeof(lora_tx_msg_t));
    rx_queue = xQueueCreate(8, sizeof(lora_rx_msg_t));

    if (!tx_queue || !rx_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    ESP_LOGI(TAG, "Message queues created: TX=%d, RX=%d", 8, 8);

    // 创建 LoRa 任务
    BaseType_t ret = xTaskCreate(lora_task, "lora_task", 8192, NULL, 10, &lora_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LoRa task");
        return;
    }

    ESP_LOGI(TAG, "E22-400M22S module initialized successfully");
}

QueueHandle_t e22_get_tx_queue(void) {
    return tx_queue;
}

QueueHandle_t e22_get_rx_queue(void) {
    return rx_queue;
}

e22_mode_t e22_get_mode(void) {
    return g_mode;
}

/* ==================== 休眠与唤醒实现 ==================== */

// 手动唤醒 SX1268 (通过拉低 NSS 触发 NSS 下降沿)
void e22_wakeup(void) {
    if (g_mode != E22_MODE_SLEEP) {
        return;
    }
    
    ESP_LOGI(TAG, "Waking up SX1268 via NSS low pulse...");
    
    // 1. 通过拉低 NSS 唤醒 SX1268
    spi_nss_low();
    vTaskDelay(pdMS_TO_TICKS(20));
    spi_nss_high();
    
    // 2. 等待 BUSY 引脚变低 (表示 SX1268 芯片内部 RC 振荡器起振并准备就绪)
    wait_busy(1000);

    // 3. 唤醒后，重新设置 Standby 模式并恢复 RF 寄存器配置
    sx1268_set_standby(SX1268_STANDBY_RC);
    e22_configure();
    e22_start_rx();
    
    ESP_LOGI(TAG, "SX1268 Wakeup complete.");
}

// 供外部或任务调用的休眠函数
esp_err_t e22_sleep(uint32_t sleep_time_ms) {
    ESP_LOGI(TAG, "Entering sleep mode, duration: %lu ms...", sleep_time_ms);

    // 1. 关闭 RF 开关，防止泄漏电流
    rf_switch_idle();

    // 2. 先切到 Standby 模式
    sx1268_set_standby(SX1268_STANDBY_RC);
    vTaskDelay(pdMS_TO_TICKS(2));

    // 3. 发送 SetSleep 命令 (0x04 表示 Warm Start，唤醒时保留 SRAM 配置数据)
    sx1268_set_sleep(SX1268_SLEEP_WARM_START);
    g_mode = E22_MODE_SLEEP;

    // 4. 如果传入参数大于 0，由主控等待指定时间后自动唤醒
    if (sleep_time_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(sleep_time_ms));
        e22_wakeup();
    } else {
        ESP_LOGI(TAG, "E22 set to infinite sleep, call e22_wakeup() to wake up.");
    }

    return ESP_OK;
}

/* ==================== 消息队列伪函数 (预留接口, 供上层应用实现) ==================== */

/*
 * 以下是供其他任务调用的消息队列封装伪函数.
 * 已在头文件中声明 (通过队列句柄可直接操作),
 * 此处提供便利封装示例, 可根据实际业务需求扩展.
 */

// 向发送队列投递一条消息 (非阻塞, 伪封装)
// 外部任务调用方式:  lora_tx_msg_t msg = { .data = "hello", .len = 5 };
//                      e22_send_msg(&msg);
bool e22_send_msg(const lora_tx_msg_t *msg) {
    if (!tx_queue || !msg || msg->len == 0 ) {
        return false;
    }
    return (pdTRUE == xQueueSend(tx_queue, msg, pdMS_TO_TICKS(100)));
}

// 从接收队列获取一条消息 (非阻塞, 伪封装)
// 外部任务调用方式:  lora_rx_msg_t msg;
//                      if (e22_recv_msg(&msg)) { ... 处理 ... }
bool e22_recv_msg(lora_rx_msg_t *msg) {
    if (!rx_queue || !msg) {
        return false;
    }
    return (pdTRUE == xQueueReceive(rx_queue, msg, 0));
}

// 向发送队列投递一条消息 (阻塞, 伪封装)
bool e22_send_msg_blocking(const lora_tx_msg_t *msg, TickType_t timeout_ticks) {
    if (!tx_queue || !msg || msg->len == 0) {
        return false;
    }
    return (pdTRUE == xQueueSend(tx_queue, msg, timeout_ticks));
}

// 从接收队列获取一条消息 (阻塞, 伪封装)
bool e22_recv_msg_blocking(lora_rx_msg_t *msg, TickType_t timeout_ticks) {
    if (!rx_queue || !msg) {
        return false;
    }
    return (pdTRUE == xQueueReceive(rx_queue, msg, timeout_ticks));
}
