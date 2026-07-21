#pragma once

#include <stdbool.h>

#include "esp_err.h"

/** 温湿度服务向上层提供的最新采样结果。 */
typedef struct {
    float temperature_c;
    float humidity_rh;
    bool valid;
    esp_err_t last_error;
} environment_sensor_data_t;

/**
 * @brief 启动 AHT20 温湿度采样服务。
 *
 * 服务在独立任务中立即采样一次，之后每 10 秒更新一次缓存。
 */
esp_err_t environment_sensor_init(void);

/**
 * @brief 获取最近一次温湿度采样结果，不会触发 I2C 访问。
 */
esp_err_t environment_sensor_get_latest(environment_sensor_data_t *data);
