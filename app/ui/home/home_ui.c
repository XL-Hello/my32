#include "home_ui.h"

#include <stdbool.h>
#include <time.h>

#include "controlcenter_ui.h"
#include "home_album.h"
#include "home_temp_humidity.h"
#include "lvgl/lvgl.h"
#include "platform_log.h"
#include "statusbar_ui.h"
#include "ui_font.h"

#define HOME_UI_CLOCK_REFRESH_PERIOD_MS 1000
#define HOME_UI_TAB_CONTENT_Y 120
#define HOME_UI_TAB_CONTENT_HEIGHT 148
#define HOME_UI_TAB_INDICATOR_Y 303
#define HOME_UI_TAB_PAGE_NUMBER_BOTTOM_OFFSET -3

/* 与 01-首页.png 保持一致的墨绿薄荷色板。 */
#define HOME_UI_COLOR_BACKGROUND 0x10201F
#define HOME_UI_COLOR_PRIMARY 0xF2FAF7
#define HOME_UI_COLOR_SECONDARY 0x9BB9B0
#define HOME_UI_COLOR_ACCENT 0x58D6B3
#define HOME_UI_COLOR_INDICATOR_INACTIVE 0x587870

typedef enum {
    HOME_UI_TAB_TEMP_HUMIDITY,
    HOME_UI_TAB_ALBUM,
} home_ui_tab_t;

static lv_obj_t *s_time_label;
static lv_obj_t *s_tab_content;
static lv_obj_t *s_first_tab_indicator;
static lv_obj_t *s_second_tab_indicator;
static lv_obj_t *s_page_number_label;
static lv_timer_t *s_clock_timer;
static home_ui_tab_t s_active_tab;
static bool s_tab_created;

static void home_ui_show_tab(home_ui_tab_t tab);

static lv_obj_t *home_ui_create_label(lv_obj_t *parent, const char *text,
                                      lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(HOME_UI_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *home_ui_create_tab_indicator(lv_obj_t *parent)
{
    lv_obj_t *indicator = lv_obj_create(parent);
    lv_obj_set_size(indicator, 7, 3);
    lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(indicator, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(indicator, 0, LV_PART_MAIN);
    lv_obj_clear_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(indicator, LV_OBJ_FLAG_SCROLLABLE);
    return indicator;
}

static void home_ui_update_tab_navigation(void)
{
    if (s_first_tab_indicator == NULL || s_second_tab_indicator == NULL ||
        s_page_number_label == NULL) {
        return;
    }

    const bool album_active = s_active_tab == HOME_UI_TAB_ALBUM;
    lv_obj_set_pos(s_first_tab_indicator, 100, HOME_UI_TAB_INDICATOR_Y);
    lv_obj_set_pos(s_second_tab_indicator, album_active ? 112 : 124, HOME_UI_TAB_INDICATOR_Y);
    lv_obj_set_size(s_first_tab_indicator, album_active ? 7 : 16, 3);
    lv_obj_set_size(s_second_tab_indicator, album_active ? 16 : 7, 3);
    lv_obj_set_style_bg_color(s_first_tab_indicator,
                              lv_color_hex(album_active ? HOME_UI_COLOR_INDICATOR_INACTIVE :
                                                          HOME_UI_COLOR_ACCENT),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_second_tab_indicator,
                              lv_color_hex(album_active ? HOME_UI_COLOR_ACCENT :
                                                          HOME_UI_COLOR_INDICATOR_INACTIVE),
                              LV_PART_MAIN);
    lv_label_set_text(s_page_number_label, album_active ? "2 / 2" : "1 / 2");
}

static void home_ui_create_tab_navigation(lv_obj_t *parent)
{
    s_first_tab_indicator = home_ui_create_tab_indicator(parent);
    s_second_tab_indicator = home_ui_create_tab_indicator(parent);
    s_page_number_label = home_ui_create_label(parent, "", LV_ALIGN_BOTTOM_RIGHT, -20,
                                                HOME_UI_TAB_PAGE_NUMBER_BOTTOM_OFFSET);
    lv_obj_set_style_text_color(s_page_number_label, lv_color_hex(HOME_UI_COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_page_number_label, ui_font_get_11(), LV_PART_MAIN);
}

static void home_ui_tab_gesture_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev == NULL) {
        return;
    }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT && s_active_tab == HOME_UI_TAB_TEMP_HUMIDITY) {
        log_info("switching to album tab");
        home_ui_show_tab(HOME_UI_TAB_ALBUM);
    } else if (direction == LV_DIR_RIGHT && s_active_tab == HOME_UI_TAB_ALBUM) {
        log_info("switching to temperature and humidity tab");
        home_ui_show_tab(HOME_UI_TAB_TEMP_HUMIDITY);
    }
}

static void home_ui_refresh_time(void)
{
    if (s_time_label == NULL) {
        return;
    }

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

static void home_ui_open_controlcenter_event(lv_event_t *event)
{
    (void)event;
    log_info("opening control center");
    home_ui_destroy();
    controlcenter_ui_create();
}

static void home_ui_show_tab(home_ui_tab_t tab)
{
    if (s_tab_content == NULL || (s_tab_created && s_active_tab == tab)) {
        return;
    }

    home_temp_humidity_destroy();
    lv_obj_clean(s_tab_content);

    s_active_tab = tab;
    s_tab_created = true;
    if (tab == HOME_UI_TAB_TEMP_HUMIDITY) {
        home_temp_humidity_create_tab(s_tab_content);
    } else {
        home_album_create_tab(s_tab_content);
    }
    home_ui_update_tab_navigation();
}

void home_ui_destroy(void)
{
    home_temp_humidity_destroy();
    if (s_clock_timer != NULL) {
        lv_timer_del(s_clock_timer);
        s_clock_timer = NULL;
    }
    statusbar_ui_destroy();

    s_time_label = NULL;
    s_tab_content = NULL;
    s_first_tab_indicator = NULL;
    s_second_tab_indicator = NULL;
    s_page_number_label = NULL;
    s_tab_created = false;
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
    lv_obj_add_event_cb(screen, home_ui_tab_gesture_event, LV_EVENT_GESTURE, NULL);
    statusbar_ui_create(screen, home_ui_open_controlcenter_event);

    s_time_label = home_ui_create_label(screen, "--:--", LV_ALIGN_TOP_MID, 0, 57);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, LV_PART_MAIN);

    s_tab_content = lv_obj_create(screen);
    lv_obj_set_size(s_tab_content, lv_obj_get_width(screen), HOME_UI_TAB_CONTENT_HEIGHT);
    lv_obj_set_pos(s_tab_content, 0, HOME_UI_TAB_CONTENT_Y);
    lv_obj_set_style_bg_opa(s_tab_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_tab_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_tab_content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_tab_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_tab_content, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_tab_content, home_ui_tab_gesture_event, LV_EVENT_GESTURE, NULL);

    home_ui_create_tab_navigation(screen);
    home_ui_show_tab(HOME_UI_TAB_TEMP_HUMIDITY);
    home_ui_refresh_time();
    s_clock_timer = lv_timer_create(home_ui_clock_refresh, HOME_UI_CLOCK_REFRESH_PERIOD_MS, NULL);
}

void home_ui_create_album_tab(void)
{
    home_ui_create();
    home_ui_show_tab(HOME_UI_TAB_ALBUM);
}
