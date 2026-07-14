#include "rgb_led.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "rgb_led_driver.h"

#define LOG_TAG "rgb_led"
#include "platform_log.h"

#define RGB_LED_TASK_STACK_SIZE 2048
#define RGB_LED_TASK_PRIORITY   5

static bool s_initialized;
static TaskHandle_t s_chase_task;
static SemaphoreHandle_t s_chase_stopped;

static esp_err_t rgb_led_require_ready(void)
{
    return s_initialized ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t rgb_led_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = rgb_led_driver_init();
    if (err != ESP_OK) {
        return err;
    }
    s_chase_stopped = xSemaphoreCreateBinary();
    if (s_chase_stopped == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_initialized = true;
    log_info("RGB LED initialized");
    return rgb_led_off();
}

esp_err_t rgb_led_set_rgb(uint8_t red, uint8_t yellow, uint8_t green)
{
    esp_err_t err = rgb_led_require_ready();
    if (err != ESP_OK) {
        return err;
    }
    if (s_chase_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    err = rgb_led_driver_set(red, yellow, green);
    if (err == ESP_OK) {
        log_info("RGB set: R=%u Y=%u G=%u", red, yellow, green);
    }
    return err;
}

esp_err_t rgb_led_off(void)
{
    return rgb_led_set_rgb(0, 0, 0);
}

esp_err_t rgb_led_set_color(rgb_led_color_t color)
{
    switch (color) {
    case RGB_LED_COLOR_OFF:
        return rgb_led_off();
    case RGB_LED_COLOR_RED:
        return rgb_led_set_rgb(UINT8_MAX, 0, 0);
    case RGB_LED_COLOR_YELLOW:
        return rgb_led_set_rgb(0, UINT8_MAX, 0);
    case RGB_LED_COLOR_GREEN:
        return rgb_led_set_rgb(0, 0, UINT8_MAX);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static void rgb_led_chase_task(void *arg)
{
    const TickType_t interval_ticks = (TickType_t)(uintptr_t)arg;
    const rgb_led_color_t colors[] = {
        RGB_LED_COLOR_RED,
        RGB_LED_COLOR_YELLOW,
        RGB_LED_COLOR_GREEN,
    };

    log_info("RGB chase started");
    while (true) {
        for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
            if (rgb_led_driver_set(colors[i] == RGB_LED_COLOR_RED ? UINT8_MAX : 0,
                                   colors[i] == RGB_LED_COLOR_YELLOW ? UINT8_MAX : 0,
                                   colors[i] == RGB_LED_COLOR_GREEN ? UINT8_MAX : 0) != ESP_OK) {
                log_error("Failed to set chase color");
            }
            log_info("RGB chase color: %s",
                     colors[i] == RGB_LED_COLOR_RED ? "red" :
                     colors[i] == RGB_LED_COLOR_YELLOW ? "yellow" : "green");
            if (ulTaskNotifyTake(pdTRUE, interval_ticks) != 0) {
                goto exit;
            }
        }
    }

exit:
    s_chase_task = NULL;
    xSemaphoreGive(s_chase_stopped);
    vTaskDelete(NULL);
}

esp_err_t rgb_led_start_chase(uint32_t interval_ms)
{
    esp_err_t err = rgb_led_require_ready();
    if (err != ESP_OK) {
        return err;
    }
    if (interval_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_chase_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const TickType_t interval_ticks = pdMS_TO_TICKS(interval_ms);
    if (interval_ticks == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xTaskCreate(rgb_led_chase_task, "rgb_led_chase", RGB_LED_TASK_STACK_SIZE,
                    (void *)(uintptr_t)interval_ticks, RGB_LED_TASK_PRIORITY,
                    &s_chase_task) != pdPASS) {
        s_chase_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    log_info("RGB chase task created, interval=%" PRIu32 " ms", interval_ms);
    return ESP_OK;
}

esp_err_t rgb_led_stop_chase(void)
{
    esp_err_t err = rgb_led_require_ready();
    if (err != ESP_OK) {
        return err;
    }
    if (s_chase_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskNotifyGive(s_chase_task);
    if (xSemaphoreTake(s_chase_stopped, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    log_info("RGB chase stopped");
    return rgb_led_off();
}
