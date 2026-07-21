/**
 * @file lvgl_port.c
 * @brief ESP-IDF 上的 LVGL 运行时端口。
 */

#include "lvgl_port.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_port_disp.h"
#include "home_ui.h"
#include "lvgl/lvgl.h"
#include "touch.h"

#define LVGL_TICK_PERIOD_US 2000
#define LVGL_TASK_STACK_SIZE 4096
#define LVGL_TASK_PRIORITY 5
#define TOUCH_RAW_TEST_ENABLED 1
#define UI_FRAME_PERIOD_MS 33 // 30 FPS

static esp_timer_handle_t s_lvgl_tick_timer;

static void lvgl_tick_callback(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_US / 1000);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    home_ui_create();

    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(UI_FRAME_PERIOD_MS));
    }
}

void lvgl_port_init(void)
{
    configASSERT(s_lvgl_tick_timer == NULL);

    lv_init();
    lv_port_disp_init();
#if TOUCH_RAW_TEST_ENABLED
    ESP_ERROR_CHECK(touch_init());
#endif

    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_callback,
        .name = "lvgl_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &s_lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_lvgl_tick_timer, LVGL_TICK_PERIOD_US));

    BaseType_t task_created = xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK_SIZE,
                                          NULL, LVGL_TASK_PRIORITY, NULL);
    configASSERT(task_created == pdPASS);
}
