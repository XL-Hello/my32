#pragma once

/**
 * @brief 创建全局性能监视窗口。
 *
 * 窗口位于 LVGL 顶层，不随普通页面清理而销毁；数据每秒更新一次。
 */
void system_monitor_ui_create(void);
