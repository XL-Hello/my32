#include "controlcenter_ui.h"

#include "home_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "settings_ui.h"
#include "ui_font.h"

#define CONTROL_CENTER_SETTINGS_BUTTON_SIZE 36
#define CONTROL_CENTER_SETTINGS_ICON_ZOOM LV_IMG_ZOOM_NONE
#define CONTROL_CENTER_BACK_BUTTON_SIZE 36

static void controlcenter_ui_open_settings(lv_event_t *event)
{
    (void)event;
    settings_ui_create();
}

static void controlcenter_ui_back_home(lv_event_t *event)
{
    (void)event;
    home_ui_create();
}

static void controlcenter_ui_create_settings_button(lv_obj_t *parent, const char *icon_path)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, CONTROL_CENTER_SETTINGS_BUTTON_SIZE,
                    CONTROL_CENTER_SETTINGS_BUTTON_SIZE);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, controlcenter_ui_open_settings, LV_EVENT_CLICKED, NULL);

    lv_obj_t *image = lv_img_create(button);
    ui_icon_set_src(image, icon_path);
    lv_img_set_zoom(image, CONTROL_CENTER_SETTINGS_ICON_ZOOM);
    lv_obj_set_size(image, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static void controlcenter_ui_create_back_button(lv_obj_t *parent)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, CONTROL_CENTER_BACK_BUTTON_SIZE,
                    CONTROL_CENTER_BACK_BUTTON_SIZE);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, controlcenter_ui_back_home, LV_EVENT_CLICKED, NULL);

    lv_obj_t *image = lv_img_create(button);
    ui_icon_set_src(image, UI_ICON_PATH_BACK);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

void controlcenter_ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    controlcenter_ui_create_back_button(screen);
    controlcenter_ui_create_settings_button(screen, UI_ICON_PATH_SETTINGS);
}
