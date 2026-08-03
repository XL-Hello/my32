#include "statusbar_ui.h"

#include "wifi_manager.h"

#define STATUSBAR_HEIGHT 28
#define STATUSBAR_REFRESH_PERIOD_MS 1000

static lv_obj_t *s_wifi_status_label;
static lv_timer_t *s_refresh_timer;
static lv_event_cb_t s_swipe_down_callback;

static bool statusbar_ui_wifi_is_connected(wifi_manager_status_t status)
{
    switch (status) {
    case WIFI_MANAGER_STATUS_CONNECTED_NO_IP:
    case WIFI_MANAGER_STATUS_CHECKING_INTERNET:
    case WIFI_MANAGER_STATUS_LOCAL_NETWORK_READY:
    case WIFI_MANAGER_STATUS_INTERNET_READY:
    case WIFI_MANAGER_STATUS_INTERNET_UNREACHABLE:
        return true;
    default:
        return false;
    }
}

static void statusbar_ui_refresh(lv_timer_t *timer)
{
    (void)timer;
    if (s_wifi_status_label == NULL) {
        return;
    }

    wifi_manager_network_info_t network_info;
    const bool connected = wifi_manager_get_network_info(&network_info) == ESP_OK &&
                           statusbar_ui_wifi_is_connected(network_info.status);
    if (connected) {
        lv_obj_clear_flag(s_wifi_status_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_wifi_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void statusbar_ui_gesture_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev != NULL && lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM &&
        s_swipe_down_callback != NULL) {
        s_swipe_down_callback(event);
    }
}

void statusbar_ui_destroy(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    s_wifi_status_label = NULL;
    s_swipe_down_callback = NULL;
}

void statusbar_ui_create(lv_obj_t *parent, lv_event_cb_t swipe_down_callback)
{
    statusbar_ui_destroy();

    lv_obj_t *statusbar = lv_obj_create(parent);
    lv_obj_set_size(statusbar, LV_PCT(100), STATUSBAR_HEIGHT);
    lv_obj_align(statusbar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(statusbar, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(statusbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(statusbar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(statusbar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(statusbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(statusbar, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(statusbar, LV_OBJ_FLAG_CLICKABLE);

    s_swipe_down_callback = swipe_down_callback;
    lv_obj_add_event_cb(statusbar, statusbar_ui_gesture_event, LV_EVENT_GESTURE, NULL);

    s_wifi_status_label = lv_label_create(statusbar);
    lv_label_set_text(s_wifi_status_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_wifi_status_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_wifi_status_label, LV_ALIGN_RIGHT_MID, -8, 0);

    statusbar_ui_refresh(NULL);
    s_refresh_timer = lv_timer_create(statusbar_ui_refresh, STATUSBAR_REFRESH_PERIOD_MS, NULL);
}
