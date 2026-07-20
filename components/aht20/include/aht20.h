#pragma once

#include "esp_err.h"

/**
 * @brief 初始化 AHT20 使用的 I2C 主机并确认传感器已完成校准。
 *
 * 默认使用 I2C0、GPIO15（SDA）和 GPIO16（SCL）。若硬件接线不同，可在
 * 编译时通过 AHT20_I2C_PORT、AHT20_I2C_SDA_GPIO 或 AHT20_I2C_SCL_GPIO 覆盖。
 */
esp_err_t aht20_init(void);

/**
 * @brief 触发一次测量并读取温度、相对湿度。
 *
 * @param[out] temperature_c 温度，单位为摄氏度。
 * @param[out] humidity_rh 相对湿度，单位为 %RH。
 */
esp_err_t aht20_read(float *temperature_c, float *humidity_rh);
