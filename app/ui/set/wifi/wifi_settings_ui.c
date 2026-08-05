#include "wifi_settings_ui.h"

#include <stdio.h>
#include <string.h>

#include "controlcenter_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "settings_ui.h"
#include "ui_back_button.h"
#include "ui_font.h"
#include "wifi_manager.h"

#define WIFI_SETTINGS_SCREEN_WIDTH           240
#define WIFI_SETTINGS_SCREEN_HEIGHT          320
#define WIFI_SETTINGS_REFRESH_PERIOD_MS      300
#define WIFI_SETTINGS_CARD_X                 12
#define WIFI_SETTINGS_CARD_WIDTH             216
#define WIFI_SETTINGS_STATUS_CARD_HEIGHT     46
#define WIFI_SETTINGS_NETWORK_ITEM_HEIGHT    40
#define WIFI_SETTINGS_MAX_VISIBLE_APS        WIFI_MANAGER_MAX_SCAN_RESULTS
#define WIFI_SETTINGS_ICON_SOURCE_SIZE       24
#define WIFI_SETTINGS_SWITCH_WIDTH           32
#define WIFI_SETTINGS_SWITCH_HEIGHT          18
#define WIFI_SETTINGS_SWITCH_X               184
#define WIFI_SETTINGS_SWITCH_Y               68
#define WIFI_SETTINGS_AUTHMODE_OPEN          0

#define WIFI_SETTINGS_COLOR_BACKGROUND       0x10201F
#define WIFI_SETTINGS_COLOR_CARD             0x19302E
#define WIFI_SETTINGS_COLOR_CARD_END         0x1B3733
#define WIFI_SETTINGS_COLOR_ICON_BACKGROUND  0x1D4A43
#define WIFI_SETTINGS_COLOR_ACCENT           0x58D6B3
#define WIFI_SETTINGS_COLOR_PRIMARY          0xF2FAF7
#define WIFI_SETTINGS_COLOR_SECONDARY        0x9BB9B0
#define WIFI_SETTINGS_COLOR_CONNECTED        0x7CE3C6
#define WIFI_SETTINGS_COLOR_DIVIDER          0x31534E
#define WIFI_SETTINGS_COLOR_SCROLLBAR        0x264A44
#define WIFI_SETTINGS_COLOR_OVERLAY          0x000000

typedef enum {
    WIFI_SETTINGS_UI_BACK_TO_SETTINGS,
    WIFI_SETTINGS_UI_BACK_TO_CONTROL_CENTER,
} wifi_settings_ui_back_destination_t;

static lv_obj_t *s_wifi_switch;
static lv_obj_t *s_wifi_status_label;
static lv_obj_t *s_connection_ssid_label;
static lv_obj_t *s_connection_detail_label;
static lv_obj_t *s_connection_state_label;
static lv_obj_t *s_network_list;
static lv_obj_t *s_overlay;
static lv_obj_t *s_password_textarea;
static lv_obj_t *s_keyboard;
static lv_timer_t *s_refresh_timer;
static wifi_manager_ap_info_t s_scan_results[WIFI_SETTINGS_MAX_VISIBLE_APS];
static size_t s_scan_result_count;
static bool s_waiting_for_scan;
static char s_selected_ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
static wifi_settings_ui_back_destination_t s_back_destination = WIFI_SETTINGS_UI_BACK_TO_SETTINGS;

static void wifi_settings_ui_close_overlay(void);
static void wifi_settings_ui_clear_network_list(void);
static void wifi_settings_ui_refresh_network_list(void);
static void wifi_settings_ui_refresh(lv_timer_t *timer);
static void wifi_settings_ui_connect_selected_network(void);

static void wifi_settings_ui_show_keyboard(void)
{
    if (s_keyboard != NULL && s_password_textarea != NULL) {
        lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
        lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_keyboard);
    }
}

static void wifi_settings_ui_set_image_color(lv_obj_t *image, uint32_t color)
{
    lv_obj_set_style_img_recolor(image, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *wifi_settings_ui_create_icon(lv_obj_t *parent, const char *icon_path,
                                               uint32_t color, lv_coord_t x, lv_coord_t y,
                                               lv_coord_t display_size)
{
    lv_obj_t *image = lv_img_create(parent);
    ui_icon_set_src(image, icon_path);
    wifi_settings_ui_set_image_color(image, color);
    const lv_coord_t inset = (WIFI_SETTINGS_ICON_SOURCE_SIZE - display_size) / 2;

    /*
     * LVGL 的图像缩放围绕源图中心进行。保留 24 px 的源图对象尺寸并向外扩展
     * 坐标，才能让缩放后的可见区域精确落在 (x, y, display_size) 区域中央。
     */
    lv_obj_set_size(image, WIFI_SETTINGS_ICON_SOURCE_SIZE, WIFI_SETTINGS_ICON_SOURCE_SIZE);
    lv_obj_set_pos(image, x - inset, y - inset);
    lv_img_set_pivot(image, WIFI_SETTINGS_ICON_SOURCE_SIZE / 2,
                     WIFI_SETTINGS_ICON_SOURCE_SIZE / 2);
    lv_img_set_zoom(image, (uint16_t)((uint32_t)display_size * LV_IMG_ZOOM_NONE /
                                      WIFI_SETTINGS_ICON_SOURCE_SIZE));
    return image;
}

static lv_obj_t *wifi_settings_ui_create_label(lv_obj_t *parent, const char *text,
                                                const lv_font_t *font, uint32_t color,
                                                lv_coord_t x, lv_coord_t y, lv_coord_t width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_pos(label, x, y);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *wifi_settings_ui_create_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                               lv_coord_t width, lv_coord_t height,
                                               uint32_t border_color, lv_coord_t border_width)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, width, height);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(WIFI_SETTINGS_COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(WIFI_SETTINGS_COLOR_CARD_END),
                                   LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(border_color), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, border_width, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void wifi_settings_ui_create_icon_background(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *background = lv_obj_create(parent);
    lv_obj_set_size(background, 26, 26);
    lv_obj_set_pos(background, x, y);
    lv_obj_set_style_bg_color(background, lv_color_hex(WIFI_SETTINGS_COLOR_ICON_BACKGROUND),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(background, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *wifi_settings_ui_create_text_button(lv_obj_t *parent, const char *text,
                                                      lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                                      lv_coord_t height, bool filled,
                                                      lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_bg_color(button, lv_color_hex(filled ? WIFI_SETTINGS_COLOR_ACCENT :
                                                             WIFI_SETTINGS_COLOR_CARD),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, filled ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(WIFI_SETTINGS_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, filled ? 0 : 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 5, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    if (callback != NULL) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, ui_font_get_12(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label,
                                lv_color_hex(filled ? WIFI_SETTINGS_COLOR_BACKGROUND :
                                                       WIFI_SETTINGS_COLOR_ACCENT),
                                LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

static const char *wifi_settings_ui_signal_icon_for_rssi(int8_t rssi)
{
    if (rssi >= -55) {
        return UI_ICON_PATH_WIFI_SIGNAL_STRONG;
    }
    if (rssi >= -70) {
        return UI_ICON_PATH_WIFI_SIGNAL_MEDIUM;
    }
    return UI_ICON_PATH_WIFI_SIGNAL_LOW;
}

static bool wifi_settings_ui_is_protected_network(const wifi_manager_ap_info_t *ap)
{
    return ap->authmode != WIFI_SETTINGS_AUTHMODE_OPEN;
}

static void wifi_settings_ui_delete_keyboard(void)
{
    if (s_keyboard != NULL) {
        lv_obj_del(s_keyboard);
        s_keyboard = NULL;
    }
}

static void wifi_settings_ui_close_overlay(void)
{
    if (s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_overlay != NULL) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
    }
    s_password_textarea = NULL;
}

static lv_obj_t *wifi_settings_ui_create_overlay_panel(lv_coord_t x, lv_coord_t y,
                                                        lv_coord_t width, lv_coord_t height)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, WIFI_SETTINGS_SCREEN_WIDTH, WIFI_SETTINGS_SCREEN_HEIGHT);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(WIFI_SETTINGS_COLOR_OVERLAY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    return wifi_settings_ui_create_card(s_overlay, x, y, width, height,
                                        WIFI_SETTINGS_COLOR_ACCENT, 1);
}

static void wifi_settings_ui_overlay_cancel_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_close_overlay();
}

static void wifi_settings_ui_password_event(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_FOCUSED) {
        return;
    }

    wifi_settings_ui_show_keyboard();
}

static void wifi_settings_ui_connect_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_connect_selected_network();
}

static void wifi_settings_ui_keyboard_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_READY) {
        wifi_settings_ui_connect_selected_network();
    } else if (lv_event_get_code(event) == LV_EVENT_CANCEL) {
        wifi_settings_ui_close_overlay();
    }
}

static void wifi_settings_ui_show_password_dialog(void)
{
    wifi_settings_ui_close_overlay();
    lv_obj_t *panel = wifi_settings_ui_create_overlay_panel(12, 74, 216, 110);
    wifi_settings_ui_create_label(panel, "连接 Wi-Fi", ui_font_get_14(), WIFI_SETTINGS_COLOR_PRIMARY,
                                  12, 9, 120);
    wifi_settings_ui_create_label(panel, s_selected_ssid, ui_font_get_12(),
                                  WIFI_SETTINGS_COLOR_SECONDARY, 12, 27, 192);

    s_password_textarea = lv_textarea_create(panel);
    lv_obj_set_size(s_password_textarea, 192, 30);
    lv_obj_set_pos(s_password_textarea, 12, 45);
    lv_textarea_set_placeholder_text(s_password_textarea, "输入 Wi-Fi 密码");
    lv_textarea_set_password_mode(s_password_textarea, true);
    lv_textarea_set_one_line(s_password_textarea, true);
    lv_obj_set_style_text_font(s_password_textarea, ui_font_get_12(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_password_textarea, lv_color_hex(WIFI_SETTINGS_COLOR_PRIMARY),
                                LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_password_textarea, lv_color_hex(WIFI_SETTINGS_COLOR_BACKGROUND),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_password_textarea, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_password_textarea, lv_color_hex(WIFI_SETTINGS_COLOR_ACCENT),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(s_password_textarea, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_password_textarea, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(s_password_textarea, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(s_password_textarea, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_password_textarea, wifi_settings_ui_password_event, LV_EVENT_CLICKED, NULL);

    wifi_settings_ui_create_text_button(panel, "连接", 12, 80, 84, 22, true,
                                        wifi_settings_ui_connect_event);
    wifi_settings_ui_create_text_button(panel, "取消", 108, 80, 84, 22, false,
                                        wifi_settings_ui_overlay_cancel_event);
    wifi_settings_ui_show_keyboard();
}

static void wifi_settings_ui_disconnect_event(lv_event_t *event)
{
    (void)event;
    wifi_manager_disconnect();
    wifi_settings_ui_close_overlay();
}

static void wifi_settings_ui_forget_event(lv_event_t *event)
{
    (void)event;
    wifi_manager_forget_network();
    wifi_settings_ui_close_overlay();
}

static void wifi_settings_ui_show_current_network_actions(void)
{
    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) != ESP_OK || info.ipv4[0] == '\0') {
        return;
    }

    wifi_settings_ui_close_overlay();
    lv_obj_t *panel = wifi_settings_ui_create_overlay_panel(20, 106, 200, 96);
    wifi_settings_ui_create_label(panel, "当前网络", ui_font_get_14(), WIFI_SETTINGS_COLOR_PRIMARY,
                                  12, 9, 100);
    wifi_settings_ui_create_label(panel, info.ssid, ui_font_get_12(), WIFI_SETTINGS_COLOR_SECONDARY,
                                  12, 29, 176);
    wifi_settings_ui_create_text_button(panel, "断开", 12, 60, 52, 24, true,
                                        wifi_settings_ui_disconnect_event);
    wifi_settings_ui_create_text_button(panel, "忘记", 74, 60, 52, 24, false,
                                        wifi_settings_ui_forget_event);
    wifi_settings_ui_create_text_button(panel, "取消", 136, 60, 52, 24, false,
                                        wifi_settings_ui_overlay_cancel_event);
}

static void wifi_settings_ui_current_network_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_show_current_network_actions();
}

static void wifi_settings_ui_connect_selected_network(void)
{
    if (s_selected_ssid[0] == '\0') {
        return;
    }

    const char *password = s_password_textarea == NULL ? "" :
                           lv_textarea_get_text(s_password_textarea);
    if (wifi_manager_connect(s_selected_ssid, password) == ESP_OK) {
        wifi_settings_ui_close_overlay();
        return;
    }

    if (s_password_textarea != NULL) {
        lv_textarea_set_text(s_password_textarea, "");
        lv_textarea_set_placeholder_text(s_password_textarea, "密码必须为空或为 8 至 63 个字符");
    }
}

static void wifi_settings_ui_ap_selected_event(lv_event_t *event)
{
    const wifi_manager_ap_info_t *ap = lv_event_get_user_data(event);
    if (ap == NULL) {
        return;
    }

    snprintf(s_selected_ssid, sizeof(s_selected_ssid), "%s", ap->ssid);
    if (wifi_settings_ui_is_protected_network(ap)) {
        wifi_settings_ui_show_password_dialog();
    } else {
        wifi_settings_ui_connect_selected_network();
    }
}

static void wifi_settings_ui_create_network_item(const wifi_manager_ap_info_t *ap)
{
    const bool protected_network = wifi_settings_ui_is_protected_network(ap);
    lv_obj_t *item = lv_btn_create(s_network_list);
    lv_obj_set_size(item, LV_PCT(100), WIFI_SETTINGS_NETWORK_ITEM_HEIGHT);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(item, lv_color_hex(WIFI_SETTINGS_COLOR_ICON_BACKGROUND),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(item, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(item, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(item, wifi_settings_ui_ap_selected_event, LV_EVENT_CLICKED, (void *)ap);

    wifi_settings_ui_create_label(item, ap->ssid, ui_font_get_13(), WIFI_SETTINGS_COLOR_PRIMARY,
                                  12, 3, 136);
    wifi_settings_ui_create_label(item, protected_network ? "需要密码" : "开放网络", ui_font_get_8(),
                                  WIFI_SETTINGS_COLOR_SECONDARY, 12, 22, 80);
    if (protected_network) {
        wifi_settings_ui_create_icon(item, UI_ICON_PATH_LOCK, WIFI_SETTINGS_COLOR_SECONDARY,
                                     151, 12, 16);
    }
    wifi_settings_ui_create_icon(item, wifi_settings_ui_signal_icon_for_rssi(ap->rssi),
                                 WIFI_SETTINGS_COLOR_PRIMARY, 180, 8, 20);

    lv_obj_t *divider = lv_obj_create(item);
    lv_obj_set_size(divider, 192, 1);
    lv_obj_set_pos(divider, 12, WIFI_SETTINGS_NETWORK_ITEM_HEIGHT - 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(WIFI_SETTINGS_COLOR_DIVIDER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
}

static void wifi_settings_ui_show_network_list_message(const char *message)
{
    lv_obj_t *label = wifi_settings_ui_create_label(s_network_list, message, ui_font_get_12(),
                                                    WIFI_SETTINGS_COLOR_SECONDARY, 12, 42, 192);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 42);
}

static void wifi_settings_ui_clear_network_list(void)
{
    s_scan_result_count = 0;
    s_waiting_for_scan = false;
    if (s_network_list != NULL && lv_obj_get_child_cnt(s_network_list) > 0) {
        lv_obj_clean(s_network_list);
    }
}

static void wifi_settings_ui_refresh_network_list(void)
{
    if (s_network_list == NULL) {
        return;
    }

    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) == ESP_OK && !info.enabled) {
        wifi_settings_ui_clear_network_list();
        return;
    }

    lv_obj_clean(s_network_list);
    if (s_waiting_for_scan) {
        wifi_settings_ui_show_network_list_message("正在扫描…");
        return;
    }
    if (s_scan_result_count == 0) {
        wifi_settings_ui_show_network_list_message("未发现可用热点");
        return;
    }
    for (size_t index = 0; index < s_scan_result_count; ++index) {
        wifi_settings_ui_create_network_item(&s_scan_results[index]);
    }
}

static bool wifi_settings_ui_start_scan(void)
{
    if (wifi_manager_start_scan() != ESP_OK) {
        return false;
    }
    s_waiting_for_scan = true;
    wifi_settings_ui_refresh_network_list();
    return true;
}

static void wifi_settings_ui_scan_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_start_scan();
}

static void wifi_settings_ui_toggle_event(lv_event_t *event)
{
    lv_obj_t *switch_object = lv_event_get_target(event);
    const bool enabled = lv_obj_has_state(switch_object, LV_STATE_CHECKED);
    if (wifi_manager_set_enabled(enabled) != ESP_OK) {
        wifi_settings_ui_refresh(NULL);
    } else if (!enabled) {
        /* 关闭后不保留上一轮扫描到的热点或“未发现热点”提示。 */
        wifi_settings_ui_clear_network_list();
    }
}

static void wifi_settings_ui_update_network_status(const wifi_manager_network_info_t *info)
{
    if (s_wifi_switch != NULL) {
        if (info->enabled) {
            lv_obj_add_state(s_wifi_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_wifi_switch, LV_STATE_CHECKED);
        }
    }

    if (s_wifi_status_label != NULL) {
        if (!info->enabled) {
            lv_label_set_text(s_wifi_status_label, "Wi-Fi 已关闭");
        } else if (s_waiting_for_scan) {
            lv_label_set_text(s_wifi_status_label, "正在扫描热点");
        } else if (info->ipv4[0] != '\0') {
            lv_label_set_text(s_wifi_status_label, "已开启 · 自动加入已知网络");
        } else {
            lv_label_set_text(s_wifi_status_label, wifi_manager_status_to_text(info->status));
        }
    }

    if (info->enabled && info->ipv4[0] != '\0') {
        char detail[40];
        snprintf(detail, sizeof(detail), "网络稳定 · %s", info->ipv4);
        lv_label_set_text(s_connection_ssid_label, info->ssid[0] == '\0' ? "Wi-Fi" : info->ssid);
        lv_label_set_text(s_connection_detail_label, detail);
        lv_label_set_text(s_connection_state_label, "已连接");
        lv_obj_clear_flag(s_connection_state_label, LV_OBJ_FLAG_HIDDEN);

        return;
    }

    lv_label_set_text(s_connection_ssid_label, info->ssid[0] == '\0' ? "未连接" : info->ssid);
    lv_label_set_text(s_connection_detail_label, wifi_manager_status_to_text(info->status));
    lv_obj_add_flag(s_connection_state_label, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_settings_ui_refresh(lv_timer_t *timer)
{
    (void)timer;
    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) == ESP_OK) {
        wifi_settings_ui_update_network_status(&info);
        if (!info.enabled) {
            wifi_settings_ui_clear_network_list();
            return;
        }
    }

    if (s_waiting_for_scan && !wifi_manager_is_scanning()) {
        size_t result_count = 0;
        if (wifi_manager_get_scan_results(s_scan_results, WIFI_SETTINGS_MAX_VISIBLE_APS,
                                          &result_count) == ESP_OK) {
            s_scan_result_count = result_count < WIFI_SETTINGS_MAX_VISIBLE_APS ?
                                      result_count : WIFI_SETTINGS_MAX_VISIBLE_APS;
        } else {
            s_scan_result_count = 0;
        }
        s_waiting_for_scan = false;
        wifi_settings_ui_refresh_network_list();
    }
}

static void wifi_settings_ui_back_event(lv_event_t *event)
{
    (void)event;
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    wifi_settings_ui_close_overlay();
    wifi_settings_ui_delete_keyboard();
    if (s_back_destination == WIFI_SETTINGS_UI_BACK_TO_CONTROL_CENTER) {
        controlcenter_ui_create();
    } else {
        settings_ui_create();
    }
}

static void wifi_settings_ui_create_status_card(lv_obj_t *parent)
{
    wifi_settings_ui_create_card(parent, WIFI_SETTINGS_CARD_X, 54, WIFI_SETTINGS_CARD_WIDTH,
                                 WIFI_SETTINGS_STATUS_CARD_HEIGHT, 0x29524C, 1);
    wifi_settings_ui_create_icon_background(parent, 24, 64);
    wifi_settings_ui_create_icon(parent, UI_ICON_PATH_WIFI_SIGNAL_STRONG,
                                 WIFI_SETTINGS_COLOR_ACCENT, 27, 67, 20);
    wifi_settings_ui_create_label(parent, "Wi-Fi", ui_font_get_14(), WIFI_SETTINGS_COLOR_PRIMARY,
                                  58, 61, 80);
    s_wifi_status_label = wifi_settings_ui_create_label(parent, "已开启 · 自动加入已知网络",
                                                         ui_font_get_9(),
                                                         WIFI_SETTINGS_COLOR_SECONDARY,
                                                         58, 80, 112);

    s_wifi_switch = lv_switch_create(parent);
    lv_obj_set_size(s_wifi_switch, WIFI_SETTINGS_SWITCH_WIDTH, WIFI_SETTINGS_SWITCH_HEIGHT);
    lv_obj_set_pos(s_wifi_switch, WIFI_SETTINGS_SWITCH_X, WIFI_SETTINGS_SWITCH_Y);
    lv_obj_set_ext_click_area(s_wifi_switch, 10);
    lv_obj_set_style_bg_color(s_wifi_switch, lv_color_hex(WIFI_SETTINGS_COLOR_SCROLLBAR),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_wifi_switch, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_wifi_switch, lv_color_hex(WIFI_SETTINGS_COLOR_ACCENT),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(s_wifi_switch, LV_OPA_COVER,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(s_wifi_switch, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_wifi_switch, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_wifi_switch, lv_color_hex(WIFI_SETTINGS_COLOR_PRIMARY),
                              LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_wifi_switch, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(s_wifi_switch, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_wifi_switch, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(s_wifi_switch, wifi_settings_ui_toggle_event, LV_EVENT_VALUE_CHANGED, NULL);
}

static void wifi_settings_ui_create_current_connection_card(lv_obj_t *parent)
{
    wifi_settings_ui_create_label(parent, "当前连接", ui_font_get_12(), WIFI_SETTINGS_COLOR_SECONDARY,
                                  16, 104, 80);
    lv_obj_t *card = wifi_settings_ui_create_card(parent, WIFI_SETTINGS_CARD_X, 123,
                                                  WIFI_SETTINGS_CARD_WIDTH,
                                                  WIFI_SETTINGS_STATUS_CARD_HEIGHT, 0x326D60, 1);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, wifi_settings_ui_current_network_event, LV_EVENT_CLICKED, NULL);
    wifi_settings_ui_create_icon_background(parent, 24, 133);
    wifi_settings_ui_create_icon(parent, UI_ICON_PATH_WIFI_SIGNAL_STRONG,
                                 WIFI_SETTINGS_COLOR_ACCENT, 27, 136, 20);
    s_connection_ssid_label = wifi_settings_ui_create_label(parent, "未连接", ui_font_get_14(),
                                                             WIFI_SETTINGS_COLOR_PRIMARY, 58, 129, 118);
    s_connection_detail_label = wifi_settings_ui_create_label(parent, "正在读取状态", ui_font_get_9(),
                                                               WIFI_SETTINGS_COLOR_SECONDARY, 58, 148, 118);
    s_connection_state_label = wifi_settings_ui_create_label(parent, "已连接", ui_font_get_9(),
                                                              WIFI_SETTINGS_COLOR_CONNECTED, 184, 133, 32);
    lv_obj_add_flag(s_connection_state_label, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_settings_ui_create_scan_area(lv_obj_t *parent)
{
    wifi_settings_ui_create_label(parent, "可用网络", ui_font_get_11(), WIFI_SETTINGS_COLOR_SECONDARY,
                                  16, 181, 90);
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 56, 24);
    lv_obj_set_pos(button, 172, 177);
    lv_obj_set_style_bg_color(button, lv_color_hex(WIFI_SETTINGS_COLOR_CARD_END), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(WIFI_SETTINGS_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 5, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, wifi_settings_ui_scan_event, LV_EVENT_CLICKED, NULL);
    wifi_settings_ui_create_icon(button, UI_ICON_PATH_REFRESH, WIFI_SETTINGS_COLOR_ACCENT, 7, 4, 16);
    wifi_settings_ui_create_label(button, "扫描", ui_font_get_11(), WIFI_SETTINGS_COLOR_ACCENT,
                                  27, 5, 24);
}

static void wifi_settings_ui_create_network_list(lv_obj_t *parent)
{
    s_network_list = wifi_settings_ui_create_card(parent, WIFI_SETTINGS_CARD_X, 210,
                                                  WIFI_SETTINGS_CARD_WIDTH, 100, 0x284F4A, 1);
    lv_obj_set_style_pad_all(s_network_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_network_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_network_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_network_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_network_list, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(s_network_list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(s_network_list, lv_color_hex(WIFI_SETTINGS_COLOR_ACCENT),
                              LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_network_list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(s_network_list, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(s_network_list, 0, LV_PART_SCROLLBAR);
}

static void wifi_settings_ui_create_keyboard(void)
{
    s_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_keyboard, WIFI_SETTINGS_SCREEN_WIDTH, 126);
    lv_obj_set_pos(s_keyboard, 0, 194);
    lv_obj_set_style_text_font(s_keyboard, ui_font_get_12(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_keyboard, ui_font_get_12(), LV_PART_ITEMS);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(s_keyboard, wifi_settings_ui_keyboard_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_settings_ui_create_page(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    wifi_settings_ui_close_overlay();
    wifi_settings_ui_delete_keyboard();

    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(WIFI_SETTINGS_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_back_button_create(screen, wifi_settings_ui_back_event);
    wifi_settings_ui_create_label(screen, "Wi-Fi 设置", ui_font_get_15(), WIFI_SETTINGS_COLOR_PRIMARY,
                                  54, 19, 110);
    wifi_settings_ui_create_status_card(screen);
    wifi_settings_ui_create_current_connection_card(screen);
    wifi_settings_ui_create_scan_area(screen);
    wifi_settings_ui_create_network_list(screen);
    wifi_settings_ui_create_keyboard();

    s_scan_result_count = 0;
    s_waiting_for_scan = false;
    s_selected_ssid[0] = '\0';
    wifi_settings_ui_refresh_network_list();
    s_refresh_timer = lv_timer_create(wifi_settings_ui_refresh, WIFI_SETTINGS_REFRESH_PERIOD_MS, NULL);
    wifi_settings_ui_refresh(NULL);

    wifi_manager_network_info_t info;
    if (wifi_manager_get_network_info(&info) == ESP_OK && info.enabled) {
        wifi_settings_ui_start_scan();
    }
}

void wifi_settings_ui_create(void)
{
    s_back_destination = WIFI_SETTINGS_UI_BACK_TO_SETTINGS;
    wifi_settings_ui_create_page();
}

void wifi_settings_ui_create_from_control_center(void)
{
    s_back_destination = WIFI_SETTINGS_UI_BACK_TO_CONTROL_CENTER;
    wifi_settings_ui_create_page();
}
