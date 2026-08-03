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
 * 首次调用启动 SNTP，后续调用会重新发起同步请求。该函数不阻塞等待服务器响应。
 */
esp_err_t system_time_sntp_start(void);
