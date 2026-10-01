#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc_monitor.h"

static const char *TAG = "ADC";

#define ADC_UNIT  ADC_UNIT_1  // GPIO0 = ADC1
#define ADC_CHAN  ADC_CHANNEL_0

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;

// 电池信息缓存 (由 adc_monitor_task 更新, 外部通过 get_battery_info 读取)
static int g_voltage_mv = 0;
static int g_soc = 0;
static portMUX_TYPE battery_lock = portMUX_INITIALIZER_UNLOCKED;

/* GPIO0 经过 1:2 分压；必须先把 ADC 原始码转换为毫伏。 */
static int sample_battery_mv(int count)
{
    int sum_mv = 0;
    for (int i = 0; i < count; ++i) {
        int raw = 0, pin_mv = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHAN, &raw));
        if (cali_handle)
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &pin_mv));
        else
            pin_mv = raw * 3300 / 4095;
        sum_mv += pin_mv;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return sum_mv * 2 / count;
}

static void cache_battery(int mv)
{
    int soc = get_battery_soc(mv);
    portENTER_CRITICAL(&battery_lock);
    g_voltage_mv = mv;
    g_soc = soc;
    portEXIT_CRITICAL(&battery_lock);
}

// 定义电压和电量的映射点
typedef struct {
    int voltage_mv;
    int soc;
} BatteryCurvePoint;

// 标准 4.2V 锂电池放电曲线表 (降序排列)
static const BatteryCurvePoint battery_curve[] = {
    {4200, 100},
    {4100, 90},
    {4000, 80},
    {3900, 60},
    {3800, 40},
    {3700, 20},
    {3600, 10},
    {3300, 0}
};

static const int CURVE_POINTS = sizeof(battery_curve) / sizeof(battery_curve[0]);

// 根据电压获取预估电量百分比
int get_battery_soc(int voltage_mv) {
    // 1. 超过最大值
    if (voltage_mv >= battery_curve[0].voltage_mv) {
        return 100;
    }
    // 2. 低于最小值
    if (voltage_mv <= battery_curve[CURVE_POINTS - 1].voltage_mv) {
        return 0;
    }
    
    // 3. 线性插值计算
    for (int i = 0; i < CURVE_POINTS - 1; i++) {
        if (voltage_mv <= battery_curve[i].voltage_mv && voltage_mv >= battery_curve[i+1].voltage_mv) {
            int v_max = battery_curve[i].voltage_mv;
            int v_min = battery_curve[i+1].voltage_mv;
            int soc_max = battery_curve[i].soc;
            int soc_min = battery_curve[i+1].soc;
            
            // 计算比例
            int soc = soc_min + (voltage_mv - v_min) * (soc_max - soc_min) / (v_max - v_min);
            return soc;
        }
    }
    return 0;
}

// 电压监测任务
static void adc_monitor_task(void *pvParameter) {

    // 周期性监测
    while (1) {
    // 每轮 10 次校准电压取平均，约每秒更新一次，避免低压保护延迟。
    int V_bat = sample_battery_mv(10);
    int soc = get_battery_soc(V_bat);

    // 更新全局缓存
    cache_battery(V_bat);

    //ESP_LOGI(TAG, "battery voltage: %d mV, soc: %d%%", V_bat, soc);
    vTaskDelay(pdMS_TO_TICKS(900));
    }
}

void adc_monitor_init(void) {
    ESP_LOGI(TAG, "Initializing ADC monitor...");

    // 初始化 ADC oneshot
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHAN, &chan_cfg));

    // 创建校准句柄
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t cali_ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (cali_ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration scheme created");
    } else if (cali_ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Curve fitting not supported, falling back to manual conversion");
    }

    // 启动前先同步采样一次，确保页面刷新时能立即获取到电池数据
    int boot_v = sample_battery_mv(10);
    cache_battery(boot_v);
    ESP_LOGI(TAG, "Initial battery: %d mV, soc: %d%%", boot_v, g_soc);

    // 创建监测任务
    ESP_ERROR_CHECK(xTaskCreate(adc_monitor_task, "adc_monitor", 4096, NULL, 5, NULL)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "ADC monitor task started");
}

bool get_battery_info(BatteryInfo *info) {
    if (!info) return false;
    portENTER_CRITICAL(&battery_lock);
    info->voltage_v = g_voltage_mv / 1000.0f;
    info->soc = g_soc;
    portEXIT_CRITICAL(&battery_lock);
    return true;
}

void esp_light_sleep(uint32_t sleep_ms) {
    ESP_LOGI(TAG, "Entering light sleep for %lu ms", sleep_ms);
    esp_sleep_enable_timer_wakeup(sleep_ms * 1000);
    esp_light_sleep_start();
    ESP_LOGI(TAG, "Woke up from light sleep");
}
