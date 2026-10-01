#include "ina226_monitor.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define INA_PORT I2C_NUM_0
#define INA_ADDR 0x40
#define INA_SDA GPIO_NUM_22
#define INA_SCL GPIO_NUM_19
#define INA_REG_CONFIG 0x00
#define INA_REG_SHUNT 0x01
#define INA_REG_BUS 0x02
#define INA_REG_MASK 0x06
#define INA_REG_LIMIT 0x07
#define INA_REG_MFG_ID 0xFE
#define INA_REG_DIE_ID 0xFF
#define INA_SHUNT_MOHM 10
#define INA_HARD_LIMIT_MA 7000

static const char *TAG = "INA226";
static bool s_ready;

static esp_err_t write_reg(uint8_t reg, uint16_t value)
{
    uint8_t data[] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    return i2c_master_write_to_device(INA_PORT, INA_ADDR, data, sizeof(data),
                                      pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(uint8_t reg, uint16_t *value)
{
    uint8_t bytes[2];
    esp_err_t err = i2c_master_write_read_device(INA_PORT, INA_ADDR, &reg, 1,
                                                  bytes, sizeof(bytes),
                                                  pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        *value = ((uint16_t)bytes[0] << 8) | bytes[1];
    }
    return err;
}

esp_err_t ina226_monitor_init(void)
{
    s_ready = false;
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = INA_SDA,
        .scl_io_num = INA_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t err = i2c_param_config(INA_PORT, &config);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(INA_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return err;

    uint16_t manufacturer = 0, die = 0;
    err = read_reg(INA_REG_MFG_ID, &manufacturer);
    if (err == ESP_OK) err = read_reg(INA_REG_DIE_ID, &die);
    if (err != ESP_OK || manufacturer != 0x5449 || (die & 0xFFF0) != 0x2260) {
        ESP_LOGE(TAG, "INA226 identity failed: err=%s mfg=%04x die=%04x",
                 esp_err_to_name(err), manufacturer, die);
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }

    /* 先设 7A 硬件门限：10mΩ * 7A = 70mV，INA226 分流电压每 LSB 2.5uV。 */
    err = write_reg(INA_REG_LIMIT, (INA_HARD_LIMIT_MA * INA_SHUNT_MOHM * 1000) / 2500);
    if (err == ESP_OK) err = write_reg(INA_REG_MASK, 0x8001); /* SOL + 锁存，低有效 */
    if (err == ESP_OK) err = write_reg(INA_REG_CONFIG, 0x4127); /* 连续测量，默认转换时间 */
    if (err == ESP_OK) {
        s_ready = true;
        ESP_LOGI(TAG, "INA226 ready: 10mOhm shunt, 7A hardware alert");
    }
    return err;
}

esp_err_t ina226_monitor_read(ina226_sample_t *sample)
{
    if (sample == NULL) return ESP_ERR_INVALID_ARG;
    *sample = (ina226_sample_t){0};
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    uint16_t bus, shunt;
    esp_err_t err = read_reg(INA_REG_BUS, &bus);
    if (err == ESP_OK) err = read_reg(INA_REG_SHUNT, &shunt);
    if (err != ESP_OK) return err;
    /* VBUS: 1.25mV/LSB；VSHUNT: 2.5uV/LSB。 */
    sample->bus_mv = ((int32_t)bus * 5) / 4;
    sample->current_ma = ((int32_t)(int16_t)shunt * 25) / INA_SHUNT_MOHM / 10;
    sample->over_current = sample->current_ma >= INA_HARD_LIMIT_MA;
    sample->valid = true;
    return ESP_OK;
}

esp_err_t ina226_monitor_clear_alert(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    uint16_t shunt, mask;
    esp_err_t err = read_reg(INA_REG_SHUNT, &shunt);
    if (err != ESP_OK) return err;
    if ((int16_t)shunt > 20000) return ESP_ERR_INVALID_STATE; /* >5A */
    /* INA226 锁存模式通过读取 Mask/Enable 寄存器清除 ALERT。 */
    return read_reg(INA_REG_MASK, &mask);
}
