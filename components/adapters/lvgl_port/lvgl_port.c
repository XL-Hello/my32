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
#include "lv_port_indev.h"
#include "lvgl/lvgl.h"
#include "touch.h"

#define LVGL_TICK_PERIOD_US 2000
#define LVGL_TASK_STACK_SIZE 4096
#define LVGL_TASK_PRIORITY 5
#define LVGL_IDLE_FALLBACK_DELAY_MS 10

static esp_timer_handle_t s_lvgl_tick_timer;
static lvgl_port_ui_init_cb_t s_ui_init_callback;

static TickType_t lvgl_delay_ms_to_ticks(uint32_t delay_ms)
{
    if (delay_ms == LV_NO_TIMER_READY) {
        delay_ms = LVGL_IDLE_FALLBACK_DELAY_MS;
    }

    /* FreeRTOS 当前以 10 ms 为 tick；向上取整以免错过 LVGL 的下一个定时器。 */
    const TickType_t delay_ticks =
        (TickType_t)((delay_ms + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS);
    return delay_ticks == 0 ? 1 : delay_ticks;
}

static void lvgl_tick_callback(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_US / 1000);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    if (s_ui_init_callback != NULL) {
        s_ui_init_callback();
    }

    //每次 lv_timer_handler() 返回距下个 LVGL 定时器的建议等待时间，任务按该时间动态休眠；当无定时器或返回 0 时，会安全回退为至少一个 FreeRTOS tick，避免忙等或超长阻塞。
    while (true) {
        const uint32_t delay_ms = lv_timer_handler();
        vTaskDelay(lvgl_delay_ms_to_ticks(delay_ms));
    }
}

void lvgl_port_init(lvgl_port_ui_init_cb_t ui_init_callback)
{
    configASSERT(s_lvgl_tick_timer == NULL);
    s_ui_init_callback = ui_init_callback;

    lv_init();
    lv_port_disp_init();
    ESP_ERROR_CHECK(touch_init());
    lv_port_indev_init();

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
