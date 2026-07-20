/**
 * @file lvgl_hello_world.c
 * @brief 五点触摸坐标校准界面。
 */

#include "lvgl_hello_world.h"

#include "lvgl/lvgl.h"

typedef struct {
    lv_coord_t x;
    lv_coord_t y;
    const char *number;
} calibration_target_t;

static void create_calibration_target(lv_obj_t *parent, const calibration_target_t *target)
{
    lv_obj_t *marker = lv_obj_create(parent);
    lv_obj_remove_style_all(marker);
    lv_obj_set_size(marker, 18, 18);
    lv_obj_set_pos(marker, target->x - 9, target->y - 9);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(marker, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(marker, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_border_width(marker, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_t *number = lv_label_create(marker);
    lv_label_set_text(number, target->number);
    lv_obj_set_style_text_color(number, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(number);
}

void lvgl_hello_world_create(void)
{
    static const calibration_target_t targets[] = {
        {.x = 20, .y = 20, .number = "1"},
        {.x = 219, .y = 20, .number = "2"},
        {.x = 120, .y = 160, .number = "3"},
        {.x = 20, .y = 299, .number = "4"},
        {.x = 219, .y = 299, .number = "5"},
    };

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *instruction = lv_label_create(screen);
    lv_label_set_text(instruction, "Tap targets 1 to 5 in order");
    lv_obj_set_style_text_color(instruction, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 2);

    for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
        create_calibration_target(screen, &targets[index]);
    }
}
