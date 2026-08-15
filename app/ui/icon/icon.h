#pragma once

#include <stdbool.h>

#include "lvgl/lvgl.h"

/** LittleFS 中打包的界面 PNG 图标路径。 */
#define UI_ICON_PATH_BACK                "R:/littlefs/png/back.png"             // 24*24
#define UI_ICON_PATH_ACTIVITY            "R:/littlefs/png/activity.png"         // 24*24
#define UI_ICON_PATH_CHEVRON_RIGHT       "R:/littlefs/png/chevron-right.png"    // 24*24
#define UI_ICON_PATH_DEVICE              "R:/littlefs/png/device.png"           // 24*24
#define UI_ICON_PATH_HUMIDITY            "R:/littlefs/png/humidity.png"         // 24*24
#define UI_ICON_PATH_TEMPERATURE         "R:/littlefs/png/thermometer.png"      // 24*24
#define UI_ICON_PATH_WIFI                "R:/littlefs/png/wifi.png"             // 24*24
#define UI_ICON_PATH_WIFI_SIGNAL_STRONG  "R:/littlefs/png/wifi-signal-strong.png" // 24*24
#define UI_ICON_PATH_WIFI_SIGNAL_MEDIUM  "R:/littlefs/png/wifi-signal-medium.png" // 24*24
#define UI_ICON_PATH_WIFI_SIGNAL_LOW     "R:/littlefs/png/wifi-signal-low.png"  // 24*24
#define UI_ICON_PATH_REFRESH             "R:/littlefs/png/refresh.png"          // 24*24
#define UI_ICON_PATH_LOCK                "R:/littlefs/png/lock.png"             // 24*24
#define UI_ICON_PATH_ALBUM               "R:/littlefs/png/album.png"            // 24*24
#define UI_ICON_PATH_CHIP                "R:/littlefs/png/chip.png"             // 24*24
#define UI_ICON_PATH_OS                  "R:/littlefs/png/os.png"               // 24*24
#define UI_ICON_PATH_STORAGE             "R:/littlefs/png/storage.png"          // 24*24
#define UI_ICON_PATH_UPDATE              "R:/littlefs/png/update.png"           // 24*24
#define UI_ICON_PATH_DOWNLOAD            "R:/littlefs/png/download.png"         // 24*24
#define UI_ICON_PATH_INSTALL             "R:/littlefs/png/install.png"          // 24*24
#define UI_ICON_PATH_WEATHER_CLEAR       "R:/littlefs/png/weather-clear.png"    // 24*24
#define UI_ICON_PATH_WEATHER_CLOUDY      "R:/littlefs/png/weather-cloudy.png"   // 24*24
#define UI_ICON_PATH_WEATHER_OVERCAST    "R:/littlefs/png/weather-overcast.png" // 24*24
#define UI_ICON_PATH_WEATHER_RAIN        "R:/littlefs/png/weather-rain.png"     // 24*24
#define UI_ICON_PATH_WEATHER_SNOW        "R:/littlefs/png/weather-snow.png"     // 24*24
#define UI_ICON_PATH_WEATHER_HAZE        "R:/littlefs/png/weather-haze.png"     // 24*24

#define UI_ICON_PATH_INFO                "R:/littlefs/png/info_32.png"          // 32*32
#define UI_ICON_PATH_SETTINGS            "R:/littlefs/png/settings.png"         // 32*32
#define UI_ICON_PATH_CONTROL_CENTER_WIFI "R:/littlefs/png/wifi-32.png"          // 32*32

/**
 * @brief 设置图标资源；资源不可读取时记录错误日志并清空图像对象。
 *
 * @return 资源可读取并已设置时返回 true，否则返回 false。
 */
bool ui_icon_set_src(lv_obj_t *image, const char *resource_path);
