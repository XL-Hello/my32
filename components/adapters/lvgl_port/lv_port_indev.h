/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备适配接口。
 */

#pragma once

/**
 * @brief 将已初始化的触摸控制器注册为 LVGL pointer 输入设备。
 *
 * 必须在 lv_init()、显示驱动和 touch_init() 成功后调用一次。
 */
void lv_port_indev_init(void);
