#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * @brief 启动基于 FreeRTOS 运行时间统计的整机 CPU 占用采样服务。
 *
 * 本服务统计两个核心合并后的平均忙碌率，不是单个核心占用，也不是 LVGL 的
 * `lv_timer_get_idle()` 指标。采样时通过两个 Idle 任务的运行时间计算非 Idle
 * 时间，其中包含应用任务、ESP-IDF 系统任务及影响 Idle 调度的中断负载。
 */
esp_err_t cpu_usage_init(void);

/**
 * @brief 获取最近一个采样窗口内的整机 CPU 总占用百分比。
 *
 * 计算公式为：
 * `100 - (CPU0 Idle 时间 + CPU1 Idle 时间) / (采样间隔 × CPU 核数) × 100`。
 * 例如仅一个核心满载时，双核设备的结果约为 50%；两个核心均满载时约为 100%。
 * 数据由后台任务每秒更新一次，首次有效采样前返回 0。
 */
uint8_t cpu_usage_get_percent(void);
