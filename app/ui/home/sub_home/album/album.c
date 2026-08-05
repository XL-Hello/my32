#include "album.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "album_flash.h"
#include "home_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "ui_back_button.h"
#include "ui_font.h"

#define LOG_TAG "album"
#include "platform_log.h"

#define ALBUM_VIEW_X 0
#define ALBUM_VIEW_Y 48
#define ALBUM_VIEW_WIDTH 240
#define ALBUM_VIEW_HEIGHT 240
#define ALBUM_SWITCH_BUTTON_Y 106
#define ALBUM_GESTURE_MIN_DISTANCE 24

#define ALBUM_COLOR_BACKGROUND 0x10201F
#define ALBUM_COLOR_ACCENT 0x58D6B3
#define ALBUM_COLOR_PRIMARY 0xF2FAF7
#define ALBUM_COLOR_SECONDARY 0x9BB9B0
#define ALBUM_COLOR_INDEX 0x193631
#define ALBUM_COLOR_WARNING 0xFFC857

static const uint8_t s_png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

static lv_obj_t *s_image;
static lv_obj_t *s_message_label;
static lv_obj_t *s_index_label;
static lv_obj_t *s_previous_button;
static lv_obj_t *s_next_button;
static lv_img_dsc_t s_image_descriptor;
static size_t s_current_index;
static size_t s_image_count;

static void album_clear_image_source(void)
{
    if (s_image != NULL) {
        lv_img_set_src(s_image, NULL);
    }
    memset(&s_image_descriptor, 0, sizeof(s_image_descriptor));
}

static bool album_get_png_size(const uint8_t *data, size_t data_len,
                               uint32_t *out_width, uint32_t *out_height)
{
    if (data == NULL || data_len < 24 || out_width == NULL || out_height == NULL ||
        memcmp(data, s_png_signature, sizeof(s_png_signature)) != 0) {
        return false;
    }

    const uint32_t width = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
                           ((uint32_t)data[18] << 8) | data[19];
    const uint32_t height = ((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) |
                            ((uint32_t)data[22] << 8) | data[23];
    if (width == 0 || height == 0) {
        return false;
    }

    *out_width = width;
    *out_height = height;
    return true;
}

static void album_set_message(const char *text, lv_color_t color)
{
    if (s_message_label == NULL) {
        return;
    }
    lv_label_set_text(s_message_label, text);
    lv_obj_set_style_text_color(s_message_label, color, LV_PART_MAIN);
    lv_obj_clear_flag(s_message_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(s_message_label);
}

static void album_hide_message(void)
{
    if (s_message_label != NULL) {
        lv_obj_add_flag(s_message_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool album_show_image(const uint8_t *data, size_t data_len,
                             size_t index, size_t image_count)
{
    uint32_t image_width;
    uint32_t image_height;
    if (s_image == NULL || index >= image_count ||
        !album_get_png_size(data, data_len, &image_width, &image_height)) {
        album_set_message("图片加载失败", lv_color_hex(ALBUM_COLOR_WARNING));
        return false;
    }

    if (image_width > ALBUM_VIEW_WIDTH || image_height > ALBUM_VIEW_HEIGHT) {
        album_set_message("图片加载失败", lv_color_hex(ALBUM_COLOR_WARNING));
        return false;
    }

    s_image_descriptor.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    s_image_descriptor.data_size = (uint32_t)data_len;
    s_image_descriptor.data = data;
    lv_img_set_src(s_image, &s_image_descriptor);
    lv_img_set_size_mode(s_image, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_obj_set_size(s_image, (lv_coord_t)image_width, (lv_coord_t)image_height);
    lv_img_set_zoom(s_image, LV_IMG_ZOOM_NONE);
    lv_obj_center(s_image);
    album_hide_message();
    return true;
}

static void album_update_controls(void)
{
    if (s_index_label == NULL) {
        return;
    }

    if (s_image_count == 0) {
        lv_label_set_text(s_index_label, "空");
    } else {
        lv_label_set_text_fmt(s_index_label, "#F2FAF7 %u# #9BB9B0 /# #F2FAF7 %u#",
                              (unsigned int)(s_current_index + 1),
                              (unsigned int)s_image_count);
    }

    if (s_previous_button != NULL) {
        if (s_image_count == 0 || s_current_index == 0) {
            lv_obj_add_state(s_previous_button, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_previous_button, LV_STATE_DISABLED);
        }
    }
    if (s_next_button != NULL) {
        if (s_image_count == 0 || s_current_index + 1 >= s_image_count) {
            lv_obj_add_state(s_next_button, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_next_button, LV_STATE_DISABLED);
        }
    }
}

static void album_show_index(size_t target_index)
{
    if (s_image_count == 0 || target_index >= s_image_count || target_index == s_current_index) {
        return;
    }

    album_clear_image_source();
    const uint8_t *data = NULL;
    size_t data_len = 0;
    if (!album_flash_get_image_by_index(target_index, &data, &data_len) ||
        !album_show_image(data, data_len, target_index, s_image_count)) {
        log_error("相册图片切换失败，目标索引=%u", (unsigned int)target_index);
        album_update_controls();
        return;
    }

    s_current_index = target_index;
    album_update_controls();
}

static void album_previous_event(lv_event_t *event)
{
    (void)event;
    if (s_current_index > 0) {
        album_show_index(s_current_index - 1);
    }
}

static void album_next_event(lv_event_t *event)
{
    (void)event;
    if (s_current_index + 1 < s_image_count) {
        album_show_index(s_current_index + 1);
    }
}

static void album_gesture_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev == NULL) {
        return;
    }

    lv_point_t vector;
    lv_indev_get_vect(indev, &vector);
    if (vector.x > -ALBUM_GESTURE_MIN_DISTANCE && vector.x < ALBUM_GESTURE_MIN_DISTANCE) {
        return;
    }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT) {
        album_next_event(event);
    } else if (direction == LV_DIR_RIGHT) {
        album_previous_event(event);
    }
}

static void album_back_event(lv_event_t *event)
{
    (void)event;
    album_destroy();
    lv_obj_clean(lv_scr_act());
    album_flash_deinit();
    home_ui_create_album_tab();
}

static lv_obj_t *album_create_switch_button(lv_obj_t *parent, lv_coord_t x,
                                             lv_event_cb_t callback, bool point_left)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 28, 28);
    lv_obj_set_pos(button, x, ALBUM_SWITCH_BUTTON_Y);
    lv_obj_set_style_bg_color(button, lv_color_hex(ALBUM_COLOR_INDEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_30, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(button, album_gesture_event, LV_EVENT_GESTURE, NULL);

    lv_obj_t *image = lv_img_create(button);
    ui_icon_set_src(image, UI_ICON_PATH_CHEVRON_RIGHT);
    if (point_left) {
        lv_img_set_angle(image, 1800);
    }
    lv_obj_set_style_img_recolor(image, lv_color_hex(ALBUM_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

void album_destroy(void)
{
    album_clear_image_source();
    s_image = NULL;
    s_message_label = NULL;
    s_index_label = NULL;
    s_previous_button = NULL;
    s_next_button = NULL;
    s_current_index = 0;
    s_image_count = 0;
}

void album_create(const uint8_t *data, size_t data_len, size_t image_count)
{
    album_destroy();

    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(ALBUM_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    s_image_count = image_count;

    ui_back_button_create(screen, album_back_event);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "相册");
    lv_obj_set_style_text_color(title, lv_color_hex(ALBUM_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *viewport = lv_obj_create(screen);
    lv_obj_set_size(viewport, ALBUM_VIEW_WIDTH, ALBUM_VIEW_HEIGHT);
    lv_obj_set_pos(viewport, ALBUM_VIEW_X, ALBUM_VIEW_Y);
    lv_obj_set_style_bg_color(viewport, lv_color_hex(ALBUM_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(viewport, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(viewport, false, LV_PART_MAIN);
    lv_obj_set_style_pad_all(viewport, 0, LV_PART_MAIN);
    lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(viewport, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(viewport, album_gesture_event, LV_EVENT_GESTURE, NULL);

    s_message_label = lv_label_create(viewport);
    lv_obj_set_style_text_font(s_message_label, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_add_flag(s_message_label, LV_OBJ_FLAG_HIDDEN);

    if (s_image_count == 0) {
        album_set_message("空", lv_color_hex(ALBUM_COLOR_PRIMARY));
    } else {
        s_image = lv_img_create(viewport);
        lv_obj_clear_flag(s_image, LV_OBJ_FLAG_CLICKABLE);
        if (!album_show_image(data, data_len, 0, s_image_count)) {
            log_error("默认相册图片无法显示");
        }
    }

    s_previous_button = album_create_switch_button(viewport, 1, album_previous_event, true);
    s_next_button = album_create_switch_button(viewport, 211, album_next_event, false);

    lv_obj_t *gesture_hint = lv_label_create(screen);
    lv_label_set_text(gesture_hint, "滑动切换图片");
    lv_obj_set_style_text_color(gesture_hint, lv_color_hex(ALBUM_COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(gesture_hint, ui_font_get_11(), LV_PART_MAIN);
    lv_obj_align(gesture_hint, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *index_background = lv_obj_create(screen);
    lv_obj_set_size(index_background, 56, 25);
    lv_obj_set_pos(index_background, 92, 291);
    lv_obj_set_style_bg_color(index_background, lv_color_hex(ALBUM_COLOR_INDEX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(index_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(index_background, lv_color_hex(ALBUM_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_opa(index_background, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(index_background, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(index_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(index_background, 0, LV_PART_MAIN);
    lv_obj_clear_flag(index_background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(index_background, LV_OBJ_FLAG_SCROLLABLE);

    s_index_label = lv_label_create(index_background);
    lv_label_set_recolor(s_index_label, true);
    lv_obj_set_style_text_font(s_index_label, ui_font_get_12(), LV_PART_MAIN);
    lv_obj_align(s_index_label, LV_ALIGN_CENTER, 0, 0);
    album_update_controls();
}
