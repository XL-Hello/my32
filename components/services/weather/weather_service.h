#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool valid;
    int8_t temperature_c;
    uint16_t icon_code;
    char city_name[32];
    char observed_at[32];
    int64_t updated_at_us;
    esp_err_t last_error;
} weather_snapshot_t;

/** 启动天气后台服务；未配置 Host 或 API Key 时服务保持离线占位状态。 */
esp_err_t weather_service_init(void);

/** 线程安全地获取最近一次天气快照，供 LVGL 线程只读刷新。 */
esp_err_t weather_service_get_snapshot(weather_snapshot_t *snapshot);
