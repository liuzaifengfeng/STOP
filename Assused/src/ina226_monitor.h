#ifndef INA226_MONITOR_H
#define INA226_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool valid;
    int32_t bus_mv;
    int32_t current_ma;
    bool over_current;
} ina226_sample_t;

/* Beta 网表：模组 28 脚 SDA=GPIO22，25 脚 SCL=GPIO19。 */
esp_err_t ina226_monitor_init(void);
esp_err_t ina226_monitor_read(ina226_sample_t *sample);
/* 仅在输出许可为低、分流电流已下降后清除锁存 ALERT。 */
esp_err_t ina226_monitor_clear_alert(void);

#endif
