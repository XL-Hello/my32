#include "ui_back_button.h"

#include "icon.h"

#define UI_BACK_BUTTON_TOUCH_SIZE 40
#define UI_BACK_BUTTON_VISUAL_SIZE 24
#define UI_BACK_BUTTON_MARGIN 8
#define UI_BACK_BUTTON_COLOR_SURFACE 0x1B3733
#define UI_BACK_BUTTON_COLOR_ACCENT 0x58D6B3

lv_obj_t *ui_back_button_create(lv_obj_t *parent, lv_event_cb_t event_callback)
{
    if (parent == NULL) {
        return NULL;
    }

    lv_obj_t *button = lv_btn_create(parent);
    if (button == NULL) {
        return NULL;
    }

    lv_obj_set_size(button, UI_BACK_BUTTON_TOUCH_SIZE, UI_BACK_BUTTON_TOUCH_SIZE);
    lv_obj_set_pos(button, UI_BACK_BUTTON_MARGIN, UI_BACK_BUTTON_MARGIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    if (event_callback != NULL) {
        lv_obj_add_event_cb(button, event_callback, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *circle = lv_obj_create(button);
    lv_obj_set_size(circle, UI_BACK_BUTTON_VISUAL_SIZE, UI_BACK_BUTTON_VISUAL_SIZE);
    lv_obj_center(circle);
    lv_obj_set_style_bg_color(circle, lv_color_hex(UI_BACK_BUTTON_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(circle, lv_color_hex(UI_BACK_BUTTON_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(circle, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(circle, 0, LV_PART_MAIN);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *image = lv_img_create(circle);
    ui_icon_set_src(image, UI_ICON_PATH_BACK);
    lv_obj_set_style_img_recolor(image, lv_color_hex(UI_BACK_BUTTON_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);

    return button;
}
