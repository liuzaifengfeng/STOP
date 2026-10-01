#ifndef VIN_MONITOR_H
#define VIN_MONITOR_H

#include <stdint.h>
#include "esp_err.h"

/* 临时禁用：模组返修后 VIN ADC 不可靠。修复并校准后改为 1 恢复全部检测。 */
#define VIN_MONITOR_ENABLED 0

/* 实装 R10=100k、R11=10k，VIN/11 进入 GPIO0。 */
esp_err_t vin_monitor_init(void);
esp_err_t vin_monitor_read_mv(int32_t *vin_mv);

#endif
