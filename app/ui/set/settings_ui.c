/**
 * @file settings_ui.c
 * @brief 设置菜单页。
 */

#include "settings_ui.h"

#include "home_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "wifi_settings_ui.h"
#include "ui_font.h"

#define SETTINGS_BACK_BUTTON_SIZE 36

static void settings_ui_wifi_event(lv_event_t *event)
{
    (void)event;
    wifi_settings_ui_create();
}

static void settings_ui_back_event(lv_event_t *event)
{
    (void)event;
    home_ui_create();
}

static void settings_ui_create_back_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, SETTINGS_BACK_BUTTON_SIZE, SETTINGS_BACK_BUTTON_SIZE);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, settings_ui_back_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *image = lv_img_create(button);
    ui_icon_set_src(image, UI_ICON_PATH_BACK);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

void settings_ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    settings_ui_create_back_button(screen);

    lv_obj_t *wifi_item = lv_btn_create(screen);
    lv_obj_set_size(wifi_item, 230, 48);
    lv_obj_set_pos(wifi_item, 5, 44);
    lv_obj_add_event_cb(wifi_item, settings_ui_wifi_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *wifi_label = lv_label_create(wifi_item);
    lv_label_set_text(wifi_label, "Wi-Fi");
    lv_obj_center(wifi_label);

}
