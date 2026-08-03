#include "home_ui.h"

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "lv_port_disp.h"
#include "cpu_usage.h"
#include "controlcenter_ui.h"
#include "environment_sensor.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "platform_log.h"
#include "statusbar_ui.h"
#include "ui_font.h"

#define HOME_UI_REFRESH_PERIOD_MS 10000
#define HOME_UI_CLOCK_REFRESH_PERIOD_MS 1000
#define HOME_UI_FPS_REFRESH_PERIOD_MS 250
#define HOME_UI_METRIC_ROW_WIDTH 204
#define HOME_UI_METRIC_ROW_HEIGHT 44
#define HOME_UI_METRIC_ICON_SIZE 40

/* 与 01-首页.png 保持一致的墨绿薄荷色板。 */
#define HOME_UI_COLOR_BACKGROUND 0x10201F
#define HOME_UI_COLOR_CARD       0x19302E
#define HOME_UI_COLOR_ACCENT     0x58D6B3
#define HOME_UI_COLOR_PRIMARY    0xF2FAF7
#define HOME_UI_COLOR_SECONDARY  0x9BB9B0
#define HOME_UI_COLOR_CARD_EDGE  0x2A4D47

static lv_obj_t *s_time_label;
static lv_obj_t *s_temperature_label;
static lv_obj_t *s_humidity_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_fps_label;
static lv_obj_t *s_cpu_label;
static uint32_t s_displayed_fps;
static uint8_t s_displayed_cpu_usage;
static lv_timer_t *s_refresh_timer;
static lv_timer_t *s_clock_timer;
static lv_timer_t *s_performance_timer;

static int value_to_tenths(float value)
{
    return (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
}

static void home_ui_open_controlcenter_event(lv_event_t *event)
{
    (void)event;
    log_info("opening control center");
    home_ui_destroy();
    controlcenter_ui_create();
}

void home_ui_destroy(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    if (s_clock_timer != NULL) {
        lv_timer_del(s_clock_timer);
        s_clock_timer = NULL;
    }
    if (s_performance_timer != NULL) {
        lv_timer_del(s_performance_timer);
        s_performance_timer = NULL;
    }
    statusbar_ui_destroy();
}

static void home_ui_performance_refresh(lv_timer_t *timer)
{
    (void)timer;

    const uint32_t fps = lv_port_disp_get_refresh_fps();
    if (fps != s_displayed_fps) {
        lv_label_set_text_fmt(s_fps_label, "FPS #58D6B3 %" LV_PRIu32 "#", fps);
        s_displayed_fps = fps;
    }
    const uint8_t cpu_usage = cpu_usage_get_percent();
    if (cpu_usage != s_displayed_cpu_usage) {
        lv_label_set_text_fmt(s_cpu_label, "CPU #58D6B3 %u%%#", cpu_usage);
        s_displayed_cpu_usage = cpu_usage;
    }
}

static void home_ui_refresh_time(void)
{
    time_t now = time(NULL);
    struct tm local_time;
    if (now > 1700000000 && localtime_r(&now, &local_time) != NULL) {
        lv_label_set_text_fmt(s_time_label, "%02d:%02d", local_time.tm_hour, local_time.tm_min);
    } else {
        lv_label_set_text(s_time_label, "--:--");
    }
}

static void home_ui_clock_refresh(lv_timer_t *timer)
{
    (void)timer;
    home_ui_refresh_time();
}

static void home_ui_refresh(lv_timer_t *timer)
{
    (void)timer;

    home_ui_refresh_time();

    environment_sensor_data_t sensor_data;
    if (environment_sensor_get_latest(&sensor_data) == ESP_OK && sensor_data.valid) {
        int temperature = value_to_tenths(sensor_data.temperature_c);
        int humidity = value_to_tenths(sensor_data.humidity_rh);

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

static lv_obj_t *home_ui_create_label(lv_obj_t *parent, const char *text,
                                      lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(HOME_UI_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *home_ui_create_metric_row(lv_obj_t *parent, const char *icon_path,
                                            lv_coord_t y_position)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, HOME_UI_METRIC_ROW_WIDTH, HOME_UI_METRIC_ROW_HEIGHT);
    lv_obj_set_pos(row, 36, y_position);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_background = lv_obj_create(row);
    lv_obj_set_size(icon_background, HOME_UI_METRIC_ICON_SIZE, HOME_UI_METRIC_ICON_SIZE);
    lv_obj_align(icon_background, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(icon_background, lv_color_hex(HOME_UI_COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(icon_background, lv_color_hex(HOME_UI_COLOR_CARD_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_background, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(icon_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(icon_background, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_image = lv_img_create(icon_background);
    ui_icon_set_src(icon_image, icon_path);
    lv_obj_set_style_img_recolor(icon_image, lv_color_hex(HOME_UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon_image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(icon_image);
    lv_obj_clear_flag(icon_image, LV_OBJ_FLAG_CLICKABLE);

    return home_ui_create_label(row, "--.-", LV_ALIGN_LEFT_MID,
                                HOME_UI_METRIC_ICON_SIZE + 15, 0);
}

void home_ui_create(void)
{
    home_ui_destroy();
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(HOME_UI_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    statusbar_ui_create(screen, home_ui_open_controlcenter_event);

    s_time_label = home_ui_create_label(screen, "--:--", LV_ALIGN_TOP_MID, 0, 57);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, LV_PART_MAIN);

    s_temperature_label = home_ui_create_metric_row(screen, UI_ICON_PATH_TEMPERATURE, 132);
    lv_obj_set_style_text_font(s_temperature_label, ui_font_get_20(), LV_PART_MAIN);
    s_humidity_label = home_ui_create_metric_row(screen, UI_ICON_PATH_HUMIDITY, 180);
    lv_obj_set_style_text_font(s_humidity_label, ui_font_get_20(), LV_PART_MAIN);
    s_status_label = home_ui_create_label(screen, "", LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(HOME_UI_COLOR_SECONDARY), LV_PART_MAIN);
    s_cpu_label = home_ui_create_label(screen, "CPU #58D6B3 0%#", LV_ALIGN_TOP_RIGHT, -18, 267);
    lv_label_set_recolor(s_cpu_label, true);
    lv_obj_set_style_text_font(s_cpu_label, ui_font_get_12(), LV_PART_MAIN);
    s_fps_label = home_ui_create_label(screen, "FPS #58D6B3 0#", LV_ALIGN_TOP_RIGHT, -18, 288);
    lv_label_set_recolor(s_fps_label, true);
    lv_obj_set_style_text_font(s_fps_label, ui_font_get_12(), LV_PART_MAIN);

    lv_obj_t *activity_background = lv_obj_create(screen);
    lv_obj_set_size(activity_background, HOME_UI_METRIC_ICON_SIZE, HOME_UI_METRIC_ICON_SIZE);
    lv_obj_set_pos(activity_background, 17, 264);
    lv_obj_set_style_bg_color(activity_background, lv_color_hex(HOME_UI_COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(activity_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(activity_background, lv_color_hex(HOME_UI_COLOR_CARD_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(activity_background, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(activity_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(activity_background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(activity_background, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *activity_icon = lv_img_create(activity_background);
    ui_icon_set_src(activity_icon, UI_ICON_PATH_ACTIVITY);
    lv_obj_set_style_img_recolor(activity_icon, lv_color_hex(HOME_UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(activity_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(activity_icon);
    lv_obj_clear_flag(activity_icon, LV_OBJ_FLAG_CLICKABLE);

    s_displayed_fps = UINT32_MAX;
    s_displayed_cpu_usage = UINT8_MAX;

    home_ui_refresh(NULL);
    home_ui_performance_refresh(NULL);
    s_refresh_timer = lv_timer_create(home_ui_refresh, HOME_UI_REFRESH_PERIOD_MS, NULL);
    s_clock_timer = lv_timer_create(home_ui_clock_refresh, HOME_UI_CLOCK_REFRESH_PERIOD_MS, NULL);
    s_performance_timer = lv_timer_create(home_ui_performance_refresh, HOME_UI_FPS_REFRESH_PERIOD_MS, NULL);
}
