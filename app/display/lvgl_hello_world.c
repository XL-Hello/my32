/**
 * @file lvgl_hello_world.c
 * @brief 第一个 LVGL 显示测试界面。
 */

#include "lvgl_hello_world.h"

#include "lvgl/lvgl.h"

void lvgl_hello_world_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(label);
}
