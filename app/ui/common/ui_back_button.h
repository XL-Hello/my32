#pragma once

#include "lvgl/lvgl.h"

/**
 * @brief 创建统一样式的页面返回按钮。
 *
 * 按钮固定在父对象左上角，具有 40×40 px 点击区和 24×24 px 圆形视觉区域。
 *
 * @param parent 按钮父对象。
 * @param event_callback 点击时调用的回调；可为 NULL。
 * @return 创建的按钮对象，创建失败时返回 NULL。
 */
lv_obj_t *ui_back_button_create(lv_obj_t *parent, lv_event_cb_t event_callback);
