#pragma once

#include "lvgl/lvgl.h"

/**
 * @brief 创建首页状态栏，并在状态栏内检测下滑手势。
 *
 * @param parent 状态栏的父对象。
 * @param swipe_down_callback 从状态栏下滑时调用的回调，可为 NULL。
 */
void statusbar_ui_create(lv_obj_t *parent, lv_event_cb_t swipe_down_callback);

/** @brief 销毁状态栏的刷新定时器。 */
void statusbar_ui_destroy(void);
