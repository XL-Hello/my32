#include "home_temp_humidity.h"

#include <stdint.h>
#include <stdlib.h>

#include "cpu_usage.h"
#include "environment_sensor.h"
#include "icon.h"
#include "lv_port_disp.h"
#include "lvgl/lvgl.h"
#include "platform_log.h"
#include "ui_font.h"

#define HOME_TEMP_HUMIDITY_REFRESH_PERIOD_MS 10000
#define HOME_TEMP_HUMIDITY_FPS_REFRESH_PERIOD_MS 250
#define HOME_TEMP_HUMIDITY_METRIC_ROW_X 36
#define HOME_TEMP_HUMIDITY_METRIC_ROW_WIDTH 204
#define HOME_TEMP_HUMIDITY_METRIC_ROW_HEIGHT 44
#define HOME_TEMP_HUMIDITY_ICON_SIZE 40
#define HOME_TEMP_HUMIDITY_ACTIVITY_ICON_Y 108
#define HOME_TEMP_HUMIDITY_ACTIVITY_ROW_Y (HOME_TEMP_HUMIDITY_ACTIVITY_ICON_Y - 2)
#define HOME_TEMP_HUMIDITY_METRIC_LABEL_X (HOME_TEMP_HUMIDITY_ICON_SIZE + 15)
#define HOME_TEMP_HUMIDITY_PERFORMANCE_SECOND_LINE_Y 21
#define HOME_TEMP_HUMIDITY_CPU_VALUE_WIDTH 34
#define HOME_TEMP_HUMIDITY_FPS_VALUE_WIDTH 16

#define HOME_TEMP_HUMIDITY_COLOR_CARD 0x19302E
#define HOME_TEMP_HUMIDITY_COLOR_ACCENT 0x58D6B3
#define HOME_TEMP_HUMIDITY_COLOR_PRIMARY 0xF2FAF7
#define HOME_TEMP_HUMIDITY_COLOR_SECONDARY 0x9BB9B0
#define HOME_TEMP_HUMIDITY_COLOR_CARD_EDGE 0x2A4D47

static lv_obj_t *s_temperature_label;
static lv_obj_t *s_humidity_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_fps_value_label;
static lv_obj_t *s_cpu_value_label;
static uint32_t s_displayed_fps;
static uint8_t s_displayed_cpu0_usage;
static uint8_t s_displayed_cpu1_usage;
static lv_timer_t *s_refresh_timer;
static lv_timer_t *s_performance_timer;

static int home_temp_humidity_value_to_tenths(float value)
{
    return (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
}

static lv_obj_t *home_temp_humidity_create_label(lv_obj_t *parent, const char *text,
                                                  lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *home_temp_humidity_create_metric_row(lv_obj_t *parent, const char *icon_path,
                                                       lv_coord_t y_position)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, HOME_TEMP_HUMIDITY_METRIC_ROW_WIDTH,
                    HOME_TEMP_HUMIDITY_METRIC_ROW_HEIGHT);
    lv_obj_set_pos(row, HOME_TEMP_HUMIDITY_METRIC_ROW_X, y_position);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *icon_background = lv_obj_create(row);
    lv_obj_set_size(icon_background, HOME_TEMP_HUMIDITY_ICON_SIZE,
                    HOME_TEMP_HUMIDITY_ICON_SIZE);
    lv_obj_set_pos(icon_background, 0, 2);
    lv_obj_set_style_bg_color(icon_background, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_CARD),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(icon_background,
                                  lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_CARD_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_background, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(icon_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(icon_background, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_image = lv_img_create(icon_background);
    ui_icon_set_src(icon_image, icon_path);
    lv_obj_set_style_img_recolor(icon_image, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_ACCENT),
                                 LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon_image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(icon_image);
    lv_obj_clear_flag(icon_image, LV_OBJ_FLAG_CLICKABLE);

    return home_temp_humidity_create_label(row, "--.-", LV_ALIGN_LEFT_MID,
                                           HOME_TEMP_HUMIDITY_METRIC_LABEL_X, 0);
}

static void home_temp_humidity_create_activity_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, HOME_TEMP_HUMIDITY_METRIC_ROW_WIDTH,
                    HOME_TEMP_HUMIDITY_METRIC_ROW_HEIGHT);
    lv_obj_set_pos(row, HOME_TEMP_HUMIDITY_METRIC_ROW_X, HOME_TEMP_HUMIDITY_ACTIVITY_ROW_Y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *icon_background = lv_obj_create(row);
    lv_obj_set_size(icon_background, HOME_TEMP_HUMIDITY_ICON_SIZE,
                    HOME_TEMP_HUMIDITY_ICON_SIZE);
    lv_obj_align(icon_background, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(icon_background, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_CARD),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(icon_background,
                                  lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_CARD_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_background, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(icon_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(icon_background, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_image = lv_img_create(icon_background);
    ui_icon_set_src(icon_image, UI_ICON_PATH_ACTIVITY);
    lv_obj_set_style_img_recolor(icon_image, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_ACCENT),
                                 LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon_image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(icon_image);
    lv_obj_clear_flag(icon_image, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *cpu_prefix = home_temp_humidity_create_label(
        row, "CPU:", LV_ALIGN_TOP_LEFT, HOME_TEMP_HUMIDITY_METRIC_LABEL_X, 0);
    lv_obj_t *cpu_suffix = home_temp_humidity_create_label(row, "%", LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *fps_prefix = home_temp_humidity_create_label(
        row, "FPS:", LV_ALIGN_TOP_LEFT, HOME_TEMP_HUMIDITY_METRIC_LABEL_X,
        HOME_TEMP_HUMIDITY_PERFORMANCE_SECOND_LINE_Y);

    s_cpu_value_label = home_temp_humidity_create_label(row, "00/00", LV_ALIGN_TOP_LEFT, 0, 0);
    s_fps_value_label = home_temp_humidity_create_label(row, "00", LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *performance_labels[] = {
        cpu_prefix,
        cpu_suffix,
        fps_prefix,
        s_cpu_value_label,
        s_fps_value_label,
    };
    for (size_t index = 0; index < sizeof(performance_labels) / sizeof(performance_labels[0]); ++index) {
        lv_obj_set_style_text_font(performance_labels[index], ui_font_get_12(), LV_PART_MAIN);
    }

    lv_obj_set_width(s_cpu_value_label, HOME_TEMP_HUMIDITY_CPU_VALUE_WIDTH);
    lv_obj_set_width(s_fps_value_label, HOME_TEMP_HUMIDITY_FPS_VALUE_WIDTH);
    lv_label_set_long_mode(s_cpu_value_label, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(s_fps_value_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_cpu_value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_fps_value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cpu_value_label,
                                lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_fps_value_label,
                                lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_align_to(s_cpu_value_label, cpu_prefix, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_align_to(cpu_suffix, s_cpu_value_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_align_to(s_fps_value_label, fps_prefix, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
}

static void home_temp_humidity_refresh(lv_timer_t *timer)
{
    (void)timer;
    if (s_temperature_label == NULL || s_humidity_label == NULL || s_status_label == NULL) {
        return;
    }

    environment_sensor_data_t sensor_data;
    if (environment_sensor_get_latest(&sensor_data) == ESP_OK && sensor_data.valid) {
        const int temperature = home_temp_humidity_value_to_tenths(sensor_data.temperature_c);
        const int humidity = home_temp_humidity_value_to_tenths(sensor_data.humidity_rh);

        lv_label_set_text_fmt(s_temperature_label, "%d.%d °C",
                              temperature / 10, abs(temperature % 10));
        lv_label_set_text_fmt(s_humidity_label, "%d.%d %%RH",
                              humidity / 10, abs(humidity % 10));
        lv_label_set_text(s_status_label,
                          sensor_data.last_error == ESP_OK ? "" : "Sensor update failed");
        log_info("Temperature: %.1f C, Humidity: %.1f %%RH",
                 (double)sensor_data.temperature_c, (double)sensor_data.humidity_rh);
    } else {
        lv_label_set_text(s_temperature_label, "--.- C");
        lv_label_set_text(s_humidity_label, "--.- %RH");
        lv_label_set_text(s_status_label, "Sensor unavailable");
    }
}

static void home_temp_humidity_performance_refresh(lv_timer_t *timer)
{
    (void)timer;
    if (s_fps_value_label == NULL || s_cpu_value_label == NULL) {
        return;
    }

    const uint32_t fps = lv_port_disp_get_refresh_fps();
    if (fps != s_displayed_fps) {
        lv_label_set_text_fmt(s_fps_value_label, "%02" LV_PRIu32, fps);
        s_displayed_fps = fps;
    }

    const uint8_t cpu0_usage = cpu_usage_get_core_percent(0);
    const uint8_t cpu1_usage = cpu_usage_get_core_percent(1);
    if (cpu0_usage != s_displayed_cpu0_usage || cpu1_usage != s_displayed_cpu1_usage) {
        lv_label_set_text_fmt(s_cpu_value_label, "%02u/%02u", cpu0_usage, cpu1_usage);
        s_displayed_cpu0_usage = cpu0_usage;
        s_displayed_cpu1_usage = cpu1_usage;
    }
}

void home_temp_humidity_destroy(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    if (s_performance_timer != NULL) {
        lv_timer_del(s_performance_timer);
        s_performance_timer = NULL;
    }
    s_temperature_label = NULL;
    s_humidity_label = NULL;
    s_status_label = NULL;
    s_fps_value_label = NULL;
    s_cpu_value_label = NULL;
}

void home_temp_humidity_create_tab(lv_obj_t *parent)
{
    home_temp_humidity_destroy();
    if (parent == NULL) {
        return;
    }

    s_temperature_label = home_temp_humidity_create_metric_row(parent, UI_ICON_PATH_TEMPERATURE, 12);
    lv_obj_set_style_text_font(s_temperature_label, ui_font_get_20(), LV_PART_MAIN);
    s_humidity_label = home_temp_humidity_create_metric_row(parent, UI_ICON_PATH_HUMIDITY, 60);
    lv_obj_set_style_text_font(s_humidity_label, ui_font_get_20(), LV_PART_MAIN);

    s_status_label = home_temp_humidity_create_label(parent, "", LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(HOME_TEMP_HUMIDITY_COLOR_SECONDARY),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_label, ui_font_get_11(), LV_PART_MAIN);

    home_temp_humidity_create_activity_row(parent);

    s_displayed_fps = UINT32_MAX;
    s_displayed_cpu0_usage = UINT8_MAX;
    s_displayed_cpu1_usage = UINT8_MAX;
    home_temp_humidity_refresh(NULL);
    home_temp_humidity_performance_refresh(NULL);
    s_refresh_timer = lv_timer_create(home_temp_humidity_refresh,
                                      HOME_TEMP_HUMIDITY_REFRESH_PERIOD_MS, NULL);
    s_performance_timer = lv_timer_create(home_temp_humidity_performance_refresh,
                                          HOME_TEMP_HUMIDITY_FPS_REFRESH_PERIOD_MS, NULL);
}
