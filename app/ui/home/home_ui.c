#include "home_ui.h"

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "lv_port_disp.h"
#include "cpu_usage.h"
#include "environment_sensor.h"
#include "lvgl/lvgl.h"
#include "platform_log.h"

#define HOME_UI_REFRESH_PERIOD_MS 10000
#define HOME_UI_FPS_REFRESH_PERIOD_MS 250
#define HOME_UI_PERSON_WIDTH 30
#define HOME_UI_PERSON_HEIGHT 60
#define HOME_UI_PERSON_ANIMATION_PERIOD_MS 50

static lv_obj_t *s_time_label;
static lv_obj_t *s_temperature_label;
static lv_obj_t *s_humidity_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_fps_label;
static lv_obj_t *s_cpu_label;
static lv_obj_t *s_person_canvas;
static uint32_t s_displayed_fps;
static uint8_t s_displayed_cpu_usage;
static bool s_person_step;
static lv_color_t s_person_canvas_buffer[
    LV_CANVAS_BUF_SIZE_TRUE_COLOR(HOME_UI_PERSON_WIDTH, HOME_UI_PERSON_HEIGHT)];

static int value_to_tenths(float value)
{
    return (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
}

static void home_ui_draw_person_line(const lv_draw_line_dsc_t *line_dsc,
                                     lv_coord_t x1, lv_coord_t y1,
                                     lv_coord_t x2, lv_coord_t y2)
{
    const lv_point_t points[] = {
        {.x = x1, .y = y1},
        {.x = x2, .y = y2},
    };
    lv_canvas_draw_line(s_person_canvas, points, 2, line_dsc);
}

static void home_ui_draw_person(void)
{
    const lv_coord_t vertical_offset = s_person_step ? 1 : 0;
    const lv_coord_t arm_offset = s_person_step ? 4 : -4;
    const lv_coord_t leg_offset = s_person_step ? 4 : -4;

    lv_canvas_fill_bg(s_person_canvas, lv_color_black(), LV_OPA_COVER);

    lv_draw_rect_dsc_t head_dsc;
    lv_draw_rect_dsc_init(&head_dsc);
    head_dsc.bg_color = lv_palette_main(LV_PALETTE_LIGHT_BLUE);
    head_dsc.bg_opa = LV_OPA_COVER;
    head_dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_person_canvas, 10, 2 + vertical_offset, 10, 10, &head_dsc);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_white();
    line_dsc.width = 2;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;

    home_ui_draw_person_line(&line_dsc, 15, 13 + vertical_offset, 15, 35 + vertical_offset);
    home_ui_draw_person_line(&line_dsc, 15, 20 + vertical_offset, 6, 27 + vertical_offset + arm_offset);
    home_ui_draw_person_line(&line_dsc, 15, 20 + vertical_offset, 24, 27 + vertical_offset - arm_offset);
    home_ui_draw_person_line(&line_dsc, 15, 35 + vertical_offset, 8, 52 + vertical_offset - leg_offset);
    home_ui_draw_person_line(&line_dsc, 15, 35 + vertical_offset, 22, 52 + vertical_offset + leg_offset);
    lv_obj_invalidate(s_person_canvas);
}

static void home_ui_person_animation(lv_timer_t *timer)
{
    (void)timer;
    s_person_step = !s_person_step;
    home_ui_draw_person();
}

static void home_ui_performance_refresh(lv_timer_t *timer)
{
    (void)timer;

    const uint32_t fps = lv_port_disp_get_refresh_fps();
    if (fps != s_displayed_fps) {
        lv_label_set_text_fmt(s_fps_label, "FPS %" LV_PRIu32, fps);
        s_displayed_fps = fps;
    }
    const uint8_t cpu_usage = cpu_usage_get_percent();
    if (cpu_usage != s_displayed_cpu_usage) {
        lv_label_set_text_fmt(s_cpu_label, "CPU %u%%", cpu_usage);
        s_displayed_cpu_usage = cpu_usage;
    }
}

static void home_ui_refresh(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm local_time;
    if (now > 1700000000 && localtime_r(&now, &local_time) != NULL) {
        lv_label_set_text_fmt(s_time_label, "%02d:%02d", local_time.tm_hour, local_time.tm_min);
    } else {
        lv_label_set_text(s_time_label, "--:--");
    }

    environment_sensor_data_t sensor_data;
    if (environment_sensor_get_latest(&sensor_data) == ESP_OK && sensor_data.valid) {
        int temperature = value_to_tenths(sensor_data.temperature_c);
        int humidity = value_to_tenths(sensor_data.humidity_rh);

        lv_label_set_text_fmt(s_temperature_label, "Temperature  %d.%d C",
                              temperature / 10, abs(temperature % 10));
        lv_label_set_text_fmt(s_humidity_label, "Humidity     %d.%d %%RH",
                              humidity / 10, abs(humidity % 10));
        lv_label_set_text(s_status_label,
                          sensor_data.last_error == ESP_OK ? "" : "Sensor update failed");
        log_info("Temperature: %.1f C, Humidity: %.1f %%RH",
                  (double)sensor_data.temperature_c, (double)sensor_data.humidity_rh);
    } else {
        lv_label_set_text(s_temperature_label, "Temperature  --.- C");
        lv_label_set_text(s_humidity_label, "Humidity     --.- %RH");
        lv_label_set_text(s_status_label, "Sensor unavailable");
    }
}

static lv_obj_t *home_ui_create_label(lv_obj_t *parent, const char *text,
                                      lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

void home_ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_time_label = home_ui_create_label(screen, "--:--", LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, LV_PART_MAIN);

    s_temperature_label = home_ui_create_label(screen, "Temperature  --.- C",
                                                LV_ALIGN_CENTER, 0, -22);
    s_humidity_label = home_ui_create_label(screen, "Humidity     --.- %RH",
                                             LV_ALIGN_CENTER, 0, 18);
    s_status_label = home_ui_create_label(screen, "", LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(s_status_label, lv_palette_lighten(LV_PALETTE_GREY, 2),
                                LV_PART_MAIN);
    s_fps_label = home_ui_create_label(screen, "FPS 0", LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_text_color(s_fps_label, lv_palette_lighten(LV_PALETTE_GREY, 2),
                                LV_PART_MAIN);
    s_cpu_label = home_ui_create_label(screen, "CPU 0%", LV_ALIGN_BOTTOM_RIGHT, -8, -26);
    lv_obj_set_style_text_color(s_cpu_label, lv_palette_lighten(LV_PALETTE_GREY, 2),
                                LV_PART_MAIN);
    s_displayed_fps = 0;
    s_displayed_cpu_usage = 0;

    s_person_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_person_canvas, s_person_canvas_buffer,
                         HOME_UI_PERSON_WIDTH, HOME_UI_PERSON_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_person_canvas, 8, 252);
    lv_obj_clear_flag(s_person_canvas, LV_OBJ_FLAG_SCROLLABLE);
    home_ui_draw_person();

    home_ui_refresh(NULL);
    lv_timer_create(home_ui_refresh, HOME_UI_REFRESH_PERIOD_MS, NULL);
    lv_timer_create(home_ui_performance_refresh, HOME_UI_FPS_REFRESH_PERIOD_MS, NULL);
    lv_timer_create(home_ui_person_animation, HOME_UI_PERSON_ANIMATION_PERIOD_MS, NULL);
}
