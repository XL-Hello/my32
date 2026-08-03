#include "wifi_settings_ui.h"

#include <stdio.h>
#include <string.h>

#include "icon.h"
#include "home_ui.h"
#include "lvgl/lvgl.h"
#include "settings_ui.h"
#include "ui_font.h"
#include "wifi_manager.h"

#define WIFI_SETTINGS_REFRESH_PERIOD_MS 300
#define WIFI_SETTINGS_MAX_VISIBLE_APS 10
#define WIFI_SETTINGS_BACK_BUTTON_SIZE 36

static lv_obj_t *s_status_label;
static lv_obj_t *s_network_list;
static lv_obj_t *s_selected_label;
static lv_obj_t *s_password_textarea;
static lv_obj_t *s_keyboard;
static lv_timer_t *s_refresh_timer;
static wifi_manager_ap_info_t s_scan_results[WIFI_SETTINGS_MAX_VISIBLE_APS];
static size_t s_scan_result_count;
static bool s_waiting_for_scan;
static char s_selected_ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];

static void wifi_settings_ui_delete_keyboard(void)
{
    if (s_keyboard != NULL) {
        lv_obj_del(s_keyboard);
        s_keyboard = NULL;
    }
}

static void wifi_settings_ui_ap_selected_event(lv_event_t *event)
{
    const wifi_manager_ap_info_t *ap = lv_event_get_user_data(event);
    snprintf(s_selected_ssid, sizeof(s_selected_ssid), "%s", ap->ssid);
    lv_label_set_text_fmt(s_selected_label, "已选择：%s", s_selected_ssid);
    lv_textarea_set_text(s_password_textarea, "");
}

static void wifi_settings_ui_refresh_network_list(void)
{
    lv_obj_clean(s_network_list);
    for (size_t index = 0; index < s_scan_result_count; ++index) {
        lv_obj_t *button = lv_btn_create(s_network_list);
        lv_obj_set_width(button, LV_PCT(100));
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text_fmt(label, "%s  %d dBm", s_scan_results[index].ssid,
                              s_scan_results[index].rssi);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, wifi_settings_ui_ap_selected_event, LV_EVENT_CLICKED,
                            &s_scan_results[index]);
    }
    if (s_scan_result_count == 0) {
        lv_obj_t *label = lv_label_create(s_network_list);
        lv_label_set_text(label, "未发现可用热点");
    }
}

static void wifi_settings_ui_refresh(lv_timer_t *timer)
{
    (void)timer;
    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) == ESP_OK) {
        if (info.ipv4[0] != '\0') {
            lv_label_set_text_fmt(s_status_label, "%s\n%s  %s", wifi_manager_status_to_text(info.status),
                                  info.ssid, info.ipv4);
        } else {
            lv_label_set_text(s_status_label, wifi_manager_status_to_text(info.status));
        }
    }

    if (s_waiting_for_scan && !wifi_manager_is_scanning()) {
        size_t result_count = 0;
        if (wifi_manager_get_scan_results(s_scan_results, WIFI_SETTINGS_MAX_VISIBLE_APS,
                                          &result_count) == ESP_OK) {
            s_scan_result_count = result_count < WIFI_SETTINGS_MAX_VISIBLE_APS ?
                                      result_count : WIFI_SETTINGS_MAX_VISIBLE_APS;
            wifi_settings_ui_refresh_network_list();
        }
        s_waiting_for_scan = false;
    }
}

static void wifi_settings_ui_scan_event(lv_event_t *event)
{
    (void)event;
    if (wifi_manager_start_scan() == ESP_OK) {
        s_waiting_for_scan = true;
        lv_obj_clean(s_network_list);
        lv_obj_t *label = lv_label_create(s_network_list);
        lv_label_set_text(label, "正在扫描…");
    }
}

static void wifi_settings_ui_toggle_event(lv_event_t *event)
{
    (void)event;
    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) == ESP_OK) {
        wifi_manager_set_enabled(!info.enabled);
    }
}

static void wifi_settings_ui_connect(void)
{
    if (s_selected_ssid[0] == '\0') {
        lv_label_set_text(s_selected_label, "请先选择热点");
        return;
    }
    const char *password = lv_textarea_get_text(s_password_textarea);
    if (wifi_manager_connect(s_selected_ssid, password) != ESP_OK) {
        lv_label_set_text(s_selected_label, "密码必须为空或为 8 至 63 个字符");
        return;
    }
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_settings_ui_connect_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_connect();
}

static void wifi_settings_ui_disconnect_event(lv_event_t *event)
{
    (void)event;
    wifi_manager_disconnect();
}

static void wifi_settings_ui_forget_event(lv_event_t *event)
{
    (void)event;
    wifi_manager_forget_network();
}

static void wifi_settings_ui_password_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_FOCUSED) {
        return;
    }

    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}

static void wifi_settings_ui_keyboard_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_READY) {
        wifi_settings_ui_connect();
    } else if (lv_event_get_code(event) == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_settings_ui_back_event(lv_event_t *event)
{
    (void)event;
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    wifi_settings_ui_delete_keyboard();
    settings_ui_create();
}

static lv_obj_t *wifi_settings_ui_create_button(lv_obj_t *parent, const char *text,
                                                 lv_coord_t x, lv_coord_t y,
                                                 lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 70, 28);
    lv_obj_set_pos(button, x, y);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static void wifi_settings_ui_create_back_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, WIFI_SETTINGS_BACK_BUTTON_SIZE, WIFI_SETTINGS_BACK_BUTTON_SIZE);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, wifi_settings_ui_back_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *image = lv_img_create(button);
    ui_icon_set_src(image, UI_ICON_PATH_BACK);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

void wifi_settings_ui_create(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    wifi_settings_ui_delete_keyboard();
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Wi-Fi 设置");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    wifi_settings_ui_create_back_button(screen);
    wifi_settings_ui_create_button(screen, "开/关", 85, 34, wifi_settings_ui_toggle_event);
    wifi_settings_ui_create_button(screen, "扫描", 166, 34, wifi_settings_ui_scan_event);

    s_status_label = lv_label_create(screen);
    lv_label_set_text(s_status_label, "正在读取状态");
    lv_obj_set_style_text_color(s_status_label, lv_palette_lighten(LV_PALETTE_GREY, 2),
                                LV_PART_MAIN);
    lv_obj_set_width(s_status_label, 230);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_LEFT, 5, 68);

    s_network_list = lv_obj_create(screen);
    lv_obj_set_size(s_network_list, 230, 84);
    lv_obj_set_pos(s_network_list, 5, 104);
    lv_obj_set_flex_flow(s_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_network_list, 3, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_network_list, LV_DIR_VER);

    s_selected_label = lv_label_create(screen);
    lv_label_set_text(s_selected_label, "选择热点后输入密码");
    lv_obj_set_width(s_selected_label, 230);
    lv_obj_set_style_text_color(s_selected_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(s_selected_label, 5, 194);

    s_password_textarea = lv_textarea_create(screen);
    lv_obj_set_size(s_password_textarea, 230, 34);
    lv_obj_set_pos(s_password_textarea, 5, 218);
    lv_textarea_set_placeholder_text(s_password_textarea, "Wi-Fi 密码（开放网络留空）");
    lv_textarea_set_password_mode(s_password_textarea, true);
    lv_textarea_set_one_line(s_password_textarea, true);
    lv_obj_add_flag(s_password_textarea, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_CLICKED, NULL);

    wifi_settings_ui_create_button(screen, "连接", 5, 264, wifi_settings_ui_connect_event);
    wifi_settings_ui_create_button(screen, "断开", 85, 264, wifi_settings_ui_disconnect_event);
    wifi_settings_ui_create_button(screen, "忘记", 165, 264, wifi_settings_ui_forget_event);

    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_keyboard, 240, 126);
    lv_obj_set_pos(s_keyboard, 0, 194);
    lv_obj_set_style_text_font(s_keyboard, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_keyboard, ui_font_get_16(), LV_PART_ITEMS);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    lv_obj_add_event_cb(s_keyboard, wifi_settings_ui_keyboard_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);

    s_scan_result_count = 0;
    s_waiting_for_scan = false;
    s_selected_ssid[0] = '\0';
    s_refresh_timer = lv_timer_create(wifi_settings_ui_refresh, WIFI_SETTINGS_REFRESH_PERIOD_MS, NULL);
    wifi_settings_ui_refresh(NULL);
}
