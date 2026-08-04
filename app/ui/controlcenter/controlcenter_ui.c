#include "controlcenter_ui.h"

#include <stddef.h>

#include "home_ui.h"
#include "icon.h"
#include "lvgl/lvgl.h"
#include "ui_font.h"

#define CONTROL_CENTER_HEADER_HEIGHT 61
#define CONTROL_CENTER_HEADER_BUTTON_SIZE 24
#define CONTROL_CENTER_CARD_X 13
#define CONTROL_CENTER_CARD_WIDTH 214
#define CONTROL_CENTER_CARD_HEIGHT 62
#define CONTROL_CENTER_CARD_FIRST_Y 61
#define CONTROL_CENTER_CARD_GAP 14
#define CONTROL_CENTER_CARD_ICON_CONTAINER_SIZE 48
#define CONTROL_CENTER_CARD_ICON_X 8
#define CONTROL_CENTER_CARD_LABEL_X 72
#define CONTROL_CENTER_BOTTOM_GESTURE_HEIGHT 44
#define CONTROL_CENTER_STATUS_DOT_SIZE 8

/* 与 02-控制中心.png 保持一致的墨绿薄荷色板。 */
#define CONTROL_CENTER_COLOR_BACKGROUND 0x10201F
#define CONTROL_CENTER_COLOR_CARD       0x19302E
#define CONTROL_CENTER_COLOR_CARD_END   0x1B3733
#define CONTROL_CENTER_COLOR_ACCENT     0x58D6B3
#define CONTROL_CENTER_COLOR_PRIMARY    0xF2FAF7
#define CONTROL_CENTER_COLOR_SECONDARY  0x9BB9B0
#define CONTROL_CENTER_COLOR_HEALTHY    0x22C55E

typedef struct {
    const char *icon_path;
    const char *title;
} controlcenter_ui_menu_item_t;

static const controlcenter_ui_menu_item_t s_menu_items[] = {
    {UI_ICON_PATH_SETTINGS, "设备设置"},
    {UI_ICON_PATH_CONTROL_CENTER_WIFI, "网络设置"},
    {UI_ICON_PATH_INFO, "系统信息"},
};

static void controlcenter_ui_back_home(lv_event_t *event)
{
    (void)event;
    home_ui_create();
}

static void controlcenter_ui_gesture_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev != NULL && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        controlcenter_ui_back_home(event);
    }
}

static void controlcenter_ui_set_image_color(lv_obj_t *image, uint32_t color)
{
    lv_obj_set_style_img_recolor(image, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *controlcenter_ui_create_icon(lv_obj_t *parent, const char *icon_path,
                                               uint32_t color)
{
    lv_obj_t *image = lv_img_create(parent);
    ui_icon_set_src(image, icon_path);
    controlcenter_ui_set_image_color(image, color);
    return image;
}

static void controlcenter_ui_create_header_button(lv_obj_t *parent, lv_coord_t x,
                                                   const char *icon_path,
                                                   lv_event_cb_t event_callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, CONTROL_CENTER_HEADER_BUTTON_SIZE,
                    CONTROL_CENTER_HEADER_BUTTON_SIZE);
    lv_obj_set_pos(button, x, 12);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(CONTROL_CENTER_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, event_callback, LV_EVENT_CLICKED, NULL);

    lv_obj_t *image = controlcenter_ui_create_icon(button, icon_path,
                                                    CONTROL_CENTER_COLOR_ACCENT);
    lv_obj_center(image);
}

static void controlcenter_ui_create_header(lv_obj_t *parent)
{
    controlcenter_ui_create_header_button(parent, 13, UI_ICON_PATH_BACK,
                                          controlcenter_ui_back_home);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "控制中心");
    lv_obj_set_style_text_font(title, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(CONTROL_CENTER_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 17);
}

static void controlcenter_ui_create_menu_item(lv_obj_t *parent, lv_coord_t y_position,
                                              const controlcenter_ui_menu_item_t *item)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, CONTROL_CENTER_CARD_WIDTH, CONTROL_CENTER_CARD_HEIGHT);
    lv_obj_set_pos(card, CONTROL_CENTER_CARD_X, y_position);
    lv_obj_set_style_bg_color(card, lv_color_hex(CONTROL_CENTER_COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(CONTROL_CENTER_COLOR_CARD_END), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_area = lv_obj_create(card);
    /* 容器固定 48×48，内部 PNG 按其原始尺寸（当前为 32×32）居中绘制。 */
    lv_obj_set_size(icon_area, CONTROL_CENTER_CARD_ICON_CONTAINER_SIZE,
                    CONTROL_CENTER_CARD_ICON_CONTAINER_SIZE);
    lv_obj_align(icon_area, LV_ALIGN_LEFT_MID, CONTROL_CENTER_CARD_ICON_X, 0);
    lv_obj_set_style_bg_opa(icon_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(icon_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(icon_area, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *icon = controlcenter_ui_create_icon(icon_area, item->icon_path,
                                                   CONTROL_CENTER_COLOR_ACCENT);
    lv_obj_center(icon);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, item->title);
    lv_obj_set_style_text_font(label, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(CONTROL_CENTER_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, CONTROL_CENTER_CARD_LABEL_X, 0);

    lv_obj_t *chevron = controlcenter_ui_create_icon(card, UI_ICON_PATH_CHEVRON_RIGHT,
                                                      CONTROL_CENTER_COLOR_ACCENT);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -9, 0);
}

static void controlcenter_ui_create_menu_items(lv_obj_t *parent)
{
    for (size_t index = 0; index < sizeof(s_menu_items) / sizeof(s_menu_items[0]); ++index) {
        const lv_coord_t y_position = CONTROL_CENTER_CARD_FIRST_Y +
                                      (lv_coord_t)index *
                                          (CONTROL_CENTER_CARD_HEIGHT + CONTROL_CENTER_CARD_GAP);
        controlcenter_ui_create_menu_item(parent, y_position, &s_menu_items[index]);
    }
}

static void controlcenter_ui_create_status(lv_obj_t *parent)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, CONTROL_CENTER_STATUS_DOT_SIZE, CONTROL_CENTER_STATUS_DOT_SIZE);
    lv_obj_set_pos(dot, 87, 295);
    lv_obj_set_style_bg_color(dot, lv_color_hex(CONTROL_CENTER_COLOR_HEALTHY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "设备运行正常");
    lv_obj_set_style_text_font(label, ui_font_get_12(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(CONTROL_CENTER_COLOR_SECONDARY), LV_PART_MAIN);
    lv_obj_set_pos(label, 100, 292);
}

static void controlcenter_ui_create_bottom_gesture_area(lv_obj_t *parent)
{
    lv_obj_t *gesture_area = lv_obj_create(parent);
    lv_obj_set_size(gesture_area, LV_PCT(100), CONTROL_CENTER_BOTTOM_GESTURE_HEIGHT);
    lv_obj_align(gesture_area, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(gesture_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(gesture_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gesture_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(gesture_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(gesture_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(gesture_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gesture_area, controlcenter_ui_gesture_event, LV_EVENT_GESTURE, NULL);
}

void controlcenter_ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(CONTROL_CENTER_COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen, ui_font_get_16(), LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    controlcenter_ui_create_bottom_gesture_area(screen);
    controlcenter_ui_create_header(screen);
    controlcenter_ui_create_menu_items(screen);
    controlcenter_ui_create_status(screen);
}
