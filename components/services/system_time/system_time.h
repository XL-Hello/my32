#pragma once

#include "esp_err.h"

/**
 * @brief 以固件编译时间初始化系统时钟。
 *
 * 初始时间使用编译器提供的 __DATE__ 与 __TIME__ 宏；后续可由网络校时覆盖。
 */
esp_err_t system_time_init(void);

/**
 * @brief 启动 SNTP 网络校时。
 *
 * 当前仅预留接口，尚未接入网络与 SNTP 客户端，实现会返回 ESP_ERR_NOT_SUPPORTED。
 */
esp_err_t system_time_sntp_start(void);
