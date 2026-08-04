#pragma once

#include <stdbool.h>

#include "lvgl/lvgl.h"

/** 在 LittleFS 已挂载且 LVGL 已初始化后加载 UI 字体。 */
bool ui_font_init(void);

/** 返回由 LittleFS 加载的 8 px UI 字体。 */
const lv_font_t *ui_font_get_8(void);

/** 返回由 LittleFS 加载的 9 px UI 字体。 */
const lv_font_t *ui_font_get_9(void);

/** 返回由 LittleFS 加载的 11 px UI 字体。 */
const lv_font_t *ui_font_get_11(void);

/** 返回由 LittleFS 加载的 12 px UI 字体。 */
const lv_font_t *ui_font_get_12(void);

/** 返回由 LittleFS 加载的 13 px UI 字体。 */
const lv_font_t *ui_font_get_13(void);

/** 返回由 LittleFS 加载的 14 px UI 字体。 */
const lv_font_t *ui_font_get_14(void);

/** 返回由 LittleFS 加载的 15 px UI 字体。 */
const lv_font_t *ui_font_get_15(void);

/** 返回由 LittleFS 加载的 16 px UI 字体。 */
const lv_font_t *ui_font_get_16(void);

/** 返回由 LittleFS 加载的 20 px UI 字体。 */
const lv_font_t *ui_font_get_20(void);
