#pragma once

#include "esp_log.h"

/*
 * 每个源文件可在包含本头文件前定义 LOG_TAG，例如：
 *
 * #define LOG_TAG "rgb_led"
 * #include "platform_log.h"
 *
 * 未定义时使用系统默认标签 sys。
 */
#ifndef LOG_TAG
#define LOG_TAG "sys"
#endif

/* 日志内容统一包含调用函数及源码行号。format 必须为字符串字面量。 */
#define log_error(format, ...) \
    ESP_LOGE(LOG_TAG, "(%s:%d): " format, __func__, __LINE__, ##__VA_ARGS__)
#define log_warn(format, ...) \
    ESP_LOGW(LOG_TAG, "(%s:%d): " format, __func__, __LINE__, ##__VA_ARGS__)
#define log_info(format, ...) \
    ESP_LOGI(LOG_TAG, "(%s:%d): " format, __func__, __LINE__, ##__VA_ARGS__)
#define log_debug(format, ...) \
    ESP_LOGD(LOG_TAG, "(%s:%d): " format, __func__, __LINE__, ##__VA_ARGS__)
#define log_verbose(format, ...) \
    ESP_LOGV(LOG_TAG, "(%s:%d): " format, __func__, __LINE__, ##__VA_ARGS__)
