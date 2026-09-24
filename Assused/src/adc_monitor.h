#ifndef ADC_MONITOR_H
#define ADC_MONITOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float voltage_v;  // 电池电压 (V)
    int soc;         // 电量百分比 (0-100)
} BatteryInfo;

// 初始化 ADC (GPIO0) 并创建电压监测任务
void adc_monitor_init(void);

// 获取最新的电池信息 (任务间安全读取)
bool get_battery_info(BatteryInfo *info);

/**
 * 使 ESP32 进入轻度睡眠 (light sleep)
 * CPU 和外设暂停，RAM 保持，由定时器在指定时间后唤醒
 * @param sleep_ms 睡眠时长（毫秒），唤醒后继续执行
 */
void esp_light_sleep(uint32_t sleep_ms);

#ifdef __cplusplus
}
#endif

#endif // ADC_MONITOR_H
