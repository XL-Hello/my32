#include "statusbar_ui.h"

#include "icon.h"

#define STATUSBAR_WIDTH 208
#define STATUSBAR_HEIGHT 24
#define STATUSBAR_Y_OFFSET 18
#define STATUSBAR_GESTURE_AREA_HEIGHT 52
#define STATUSBAR_SLIDER_WIDTH 173
#define STATUSBAR_DIVIDER_X 175
#define STATUSBAR_DIVIDER_HEIGHT 16
#define STATUSBAR_INITIAL_VALUE 100

/* 与 UI 稿件保持一致的墨绿薄荷色板。 */
#define UI_COLOR_CARD       0x19302E
#define UI_COLOR_ACCENT     0x58D6B3
#define UI_COLOR_ACCENT_END 0x79DCC1
#define UI_COLOR_PRIMARY    0xF2FAF7
#define UI_COLOR_TRACK_EDGE 0x2A4D47

static lv_event_cb_t s_swipe_down_callback;

static void statusbar_ui_gesture_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev != NULL && lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM &&
        s_swipe_down_callback != NULL) {
        s_swipe_down_callback(event);
    }
}

static void statusbar_ui_create_wifi_icon(lv_obj_t *parent)
{
    lv_obj_t *wifi_icon = lv_img_create(parent);
    ui_icon_set_src(wifi_icon, UI_ICON_PATH_WIFI);
    lv_obj_set_style_img_recolor(wifi_icon, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(wifi_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
}

static void statusbar_ui_create_gesture_area(lv_obj_t *parent)
{
    lv_obj_t *gesture_area = lv_obj_create(parent);
    lv_obj_set_size(gesture_area, LV_PCT(100), STATUSBAR_GESTURE_AREA_HEIGHT);
    lv_obj_align(gesture_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(gesture_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(gesture_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gesture_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(gesture_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(gesture_area, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(gesture_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gesture_area, statusbar_ui_gesture_event, LV_EVENT_GESTURE, NULL);
}

void statusbar_ui_destroy(void)
{
    s_swipe_down_callback = NULL;
}

void statusbar_ui_create(lv_obj_t *parent, lv_event_cb_t swipe_down_callback)
{
    statusbar_ui_destroy();
    s_swipe_down_callback = swipe_down_callback;

    /* 先创建透明手势区，确保顶端空白处的下滑也能进入控制中心。 */
    statusbar_ui_create_gesture_area(parent);

    lv_obj_t *statusbar = lv_obj_create(parent);
    lv_obj_set_size(statusbar, STATUSBAR_WIDTH, STATUSBAR_HEIGHT);
    lv_obj_align(statusbar, LV_ALIGN_TOP_MID, 0, STATUSBAR_Y_OFFSET);
    lv_obj_set_style_bg_color(statusbar, lv_color_hex(UI_COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(statusbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(statusbar, lv_color_hex(UI_COLOR_TRACK_EDGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(statusbar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(statusbar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(statusbar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(statusbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slider = lv_slider_create(statusbar);
    lv_obj_set_size(slider, STATUSBAR_SLIDER_WIDTH, STATUSBAR_HEIGHT - 2);
    lv_obj_set_pos(slider, 1, 1);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, STATUSBAR_INITIAL_VALUE, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(slider, statusbar_ui_gesture_event, LV_EVENT_GESTURE, NULL);

    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(slider, lv_color_hex(UI_COLOR_ACCENT_END), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);

    /* 保留可拖动的旋钮命中区，但不绘制旋钮，以免越过 Wi-Fi 分区。 */
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);

    lv_obj_t *divider = lv_obj_create(statusbar);
    lv_obj_set_size(divider, 1, STATUSBAR_DIVIDER_HEIGHT);
    lv_obj_set_pos(divider, STATUSBAR_DIVIDER_X,
                   (STATUSBAR_HEIGHT - STATUSBAR_DIVIDER_HEIGHT) / 2);
    lv_obj_set_style_bg_color(divider, lv_color_hex(UI_COLOR_TRACK_EDGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);

    statusbar_ui_create_wifi_icon(statusbar);
}
