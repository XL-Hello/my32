/**
 * @file mqtt_test_client.h
 * @brief MQTT 温湿度遥测测试客户端。
 */

#pragma once

#include "esp_err.h"

/**
 * @brief 启动 MQTT 客户端及 10 秒一次的模拟温湿度上报任务。
 *
 * Broker、主题与客户端 ID 通过 menuconfig 的“MQTT 测试”菜单配置。
 */
esp_err_t mqtt_test_client_init(void);
