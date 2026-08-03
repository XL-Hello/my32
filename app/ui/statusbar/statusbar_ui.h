#pragma once

#include "lvgl/lvgl.h"

/**
 * @brief 创建首页顶部状态滑条，并监听从顶部开始的下滑手势。
 *
 * @param parent 状态滑条的父对象。
 * @param swipe_down_callback 从顶部向下滑动时调用的回调，不需要切换页面时可为 NULL。
 */
void statusbar_ui_create(lv_obj_t *parent, lv_event_cb_t swipe_down_callback);

/** @brief 销毁状态滑条。 */
void statusbar_ui_destroy(void);
