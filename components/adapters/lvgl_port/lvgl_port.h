/**
 * @file lvgl_port.h
 * @brief LVGL 运行时端口初始化接口。
 */

#pragma once

/** @brief 在 LVGL 任务上下文创建产品 UI 的回调。 */
typedef void (*lvgl_port_ui_init_cb_t)(void);

/**
 * @brief 初始化 LVGL 核心、显示端口、tick 定时器和 LVGL 处理任务。
 *
 * 此函数只能调用一次。所有 LVGL API 均应由 LVGL 任务调用。
 */
void lvgl_port_init(lvgl_port_ui_init_cb_t ui_init_callback);
