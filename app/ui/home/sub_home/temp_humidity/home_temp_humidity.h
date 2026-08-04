#pragma once

#include "lvgl/lvgl.h"

/** 在首页内容区域中创建温湿度、性能和活动状态 Tab。 */
void home_temp_humidity_create_tab(lv_obj_t *parent);

/** 停止本 Tab 的定时器并清空已创建控件的引用。 */
void home_temp_humidity_destroy(void);
