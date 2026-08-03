#pragma once

#include <stdbool.h>

#include "lvgl/lvgl.h"

/** LittleFS 中打包的界面 PNG 图标路径。 */
#define UI_ICON_PATH_BACK        "R:/littlefs/png/back.png"
#define UI_ICON_PATH_ACTIVITY    "R:/littlefs/png/activity.png"
#define UI_ICON_PATH_HUMIDITY    "R:/littlefs/png/humidity.png"
#define UI_ICON_PATH_SETTINGS    "R:/littlefs/png/seticon.png"
#define UI_ICON_PATH_TEMPERATURE "R:/littlefs/png/thermometer.png"
#define UI_ICON_PATH_WIFI        "R:/littlefs/png/wifi.png"

/**
 * @brief 设置图标资源；资源不可读取时记录错误日志并清空图像对象。
 *
 * @return 资源可读取并已设置时返回 true，否则返回 false。
 */
bool ui_icon_set_src(lv_obj_t *image, const char *resource_path);
