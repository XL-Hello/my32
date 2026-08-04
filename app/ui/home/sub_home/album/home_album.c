#include "home_album.h"

#include "album.h"
#include "album_flash.h"
#include "home_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "ui_font.h"

#define LOG_TAG "home_album"
#include "platform_log.h"

#define HOME_ALBUM_COLOR_CARD 0x19302E
#define HOME_ALBUM_COLOR_CARD_EDGE 0x2A4D47
#define HOME_ALBUM_COLOR_ACCENT 0x58D6B3
#define HOME_ALBUM_COLOR_PRIMARY 0xF2FAF7
#define HOME_ALBUM_COLOR_SECONDARY 0x9BB9B0
#define HOME_ALBUM_COLOR_RING 0x173430
#define HOME_ALBUM_COLOR_INNER_RING 0x1C4B43

static void home_album_open_event(lv_event_t *event)
{
    (void)event;

    if (!album_flash_init()) {
        log_error("相册资源初始化失败，将显示读取错误状态");
    }

    const size_t image_count = album_flash_get_image_count();
    const album_flash_image_t *current_image = album_flash_get_current_image();
    home_ui_destroy();
    album_create(current_image != NULL ? current_image->data : NULL,
                 current_image != NULL ? current_image->data_len : 0,
                 image_count);
}

static void home_album_create_entry(lv_obj_t *parent)
{
    lv_obj_t *entry = lv_btn_create(parent);
    lv_obj_set_size(entry, 88, 122);
    lv_obj_set_pos(entry, 76, 10);
    lv_obj_set_style_bg_opa(entry, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(entry, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(entry, 0, LV_PART_MAIN);
    lv_obj_clear_flag(entry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(entry, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(entry, home_album_open_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *outer_ring = lv_obj_create(entry);
    lv_obj_set_size(outer_ring, 76, 76);
    lv_obj_set_pos(outer_ring, 6, 6);
    lv_obj_set_style_bg_color(outer_ring, lv_color_hex(HOME_ALBUM_COLOR_RING), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(outer_ring, lv_color_hex(HOME_ALBUM_COLOR_CARD_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(outer_ring, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(outer_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(outer_ring, 0, LV_PART_MAIN);
    lv_obj_clear_flag(outer_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(outer_ring, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inner_ring = lv_obj_create(entry);
    lv_obj_set_size(inner_ring, 58, 58);
    lv_obj_set_pos(inner_ring, 15, 15);
    lv_obj_set_style_bg_color(inner_ring, lv_color_hex(HOME_ALBUM_COLOR_INNER_RING), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(inner_ring, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(inner_ring, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(inner_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(inner_ring, 0, LV_PART_MAIN);
    lv_obj_clear_flag(inner_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(inner_ring, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_img_create(entry);
    ui_icon_set_src(icon, UI_ICON_PATH_ALBUM);
    /* 源图为 24 px，围绕中心放大到设计稿要求的 42 px。 */
    lv_img_set_size_mode(icon, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_obj_set_size(icon, 24, 24);
    lv_img_set_pivot(icon, 12, 12);
    lv_img_set_zoom(icon, 448);
    lv_img_set_antialias(icon, true);
    lv_obj_set_pos(icon, 32, 32);
    lv_obj_set_style_img_recolor(icon, lv_color_hex(HOME_ALBUM_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(entry);
    lv_label_set_text(title, "相册");
    lv_obj_set_style_text_color(title, lv_color_hex(HOME_ALBUM_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 87);

    lv_obj_t *description = lv_label_create(entry);
    lv_label_set_text(description, "查看精彩瞬间");
    lv_obj_set_style_text_color(description, lv_color_hex(HOME_ALBUM_COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(description, ui_font_get_11(), LV_PART_MAIN);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 108);
}

void home_album_create_tab(lv_obj_t *parent)
{
    if (parent == NULL) {
        return;
    }

    home_album_create_entry(parent);
}
