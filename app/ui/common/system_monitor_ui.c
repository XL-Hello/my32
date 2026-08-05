#include "system_monitor_ui.h"

#include <stdint.h>

#include "cpu_usage.h"
#include "lv_port_disp.h"
#include "lvgl/lvgl.h"
#include "ui_font.h"

#define SYSTEM_MONITOR_REFRESH_PERIOD_MS 1000
#define SYSTEM_MONITOR_WINDOW_X 4
#define SYSTEM_MONITOR_WINDOW_Y 2
#define SYSTEM_MONITOR_WINDOW_WIDTH 112
#define SYSTEM_MONITOR_WINDOW_HEIGHT 14

#define SYSTEM_MONITOR_COLOR_BACKGROUND 0x19302E
#define SYSTEM_MONITOR_COLOR_BORDER 0x2A4D47
#define SYSTEM_MONITOR_COLOR_TEXT 0x58D6B3

static lv_obj_t *s_monitor_label;

static void system_monitor_ui_refresh(lv_timer_t *timer)
{
    (void)timer;
    if (s_monitor_label == NULL) {
        return;
    }

    lv_label_set_text_fmt(s_monitor_label, "CPU:%02u/%02u FPS:%02" LV_PRIu32,
                          cpu_usage_get_core_percent(0), cpu_usage_get_core_percent(1),
                          lv_port_disp_get_refresh_fps());
}

void system_monitor_ui_create(void)
{
    if (s_monitor_label != NULL) {
        return;
    }

    lv_obj_t *window = lv_obj_create(lv_layer_top());
    lv_obj_set_size(window, SYSTEM_MONITOR_WINDOW_WIDTH, SYSTEM_MONITOR_WINDOW_HEIGHT);
    lv_obj_set_pos(window, SYSTEM_MONITOR_WINDOW_X, SYSTEM_MONITOR_WINDOW_Y);
    lv_obj_set_style_bg_color(window, lv_color_hex(SYSTEM_MONITOR_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(window, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(window, lv_color_hex(SYSTEM_MONITOR_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(window, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(window, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(window, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(window, 0, LV_PART_MAIN);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_GESTURE_BUBBLE);

    s_monitor_label = lv_label_create(window);
    lv_obj_set_width(s_monitor_label, LV_PCT(100));
    lv_obj_set_style_text_font(s_monitor_label, ui_font_get_8(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_monitor_label, lv_color_hex(SYSTEM_MONITOR_COLOR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_monitor_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(s_monitor_label);

    system_monitor_ui_refresh(NULL);
    lv_timer_create(system_monitor_ui_refresh, SYSTEM_MONITOR_REFRESH_PERIOD_MS, NULL);
}
