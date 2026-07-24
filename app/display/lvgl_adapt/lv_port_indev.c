/**
 * @file lv_port_indev.c
 * @brief 将 HR2046 触摸点适配为 LVGL pointer 输入设备。
 */

#include "lv_port_indev.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "lvgl/lvgl.h"
#include "touch.h"

#define LOG_TAG "lvgl_indev"
#include "platform_log.h"

static lv_indev_drv_t s_touch_indev_drv;
static lv_indev_t *s_touch_indev;

static void touchpad_read_callback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    data->continue_reading = false;

    touch_point_t point;
    const esp_err_t err = touch_read_point(&point);
    if (err != ESP_OK) {
        /* 本次采样失败时释放指针，避免 LVGL 保持在错误的按下状态。 */
        data->state = LV_INDEV_STATE_RELEASED;
        log_error("touch read failed: %s", esp_err_to_name(err));
        return;
    }

    if (!point.pressed || !point.valid) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
}

void lv_port_indev_init(void)
{
    configASSERT(s_touch_indev == NULL);

    lv_indev_drv_init(&s_touch_indev_drv);
    s_touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_touch_indev_drv.read_cb = touchpad_read_callback;
    s_touch_indev = lv_indev_drv_register(&s_touch_indev_drv);
    configASSERT(s_touch_indev != NULL);

    log_info("LVGL pointer input registered: gesture_limit=%d, min_velocity=%d",
             s_touch_indev_drv.gesture_limit, s_touch_indev_drv.gesture_min_velocity);
}
