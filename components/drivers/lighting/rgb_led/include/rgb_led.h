#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    RGB_LED_COLOR_OFF = 0,
    RGB_LED_COLOR_RED,
    RGB_LED_COLOR_YELLOW,
    RGB_LED_COLOR_GREEN,
} rgb_led_color_t;

esp_err_t rgb_led_init(void);
esp_err_t rgb_led_off(void);
esp_err_t rgb_led_set_rgb(uint8_t red, uint8_t yellow, uint8_t green);
esp_err_t rgb_led_set_color(rgb_led_color_t color);
esp_err_t rgb_led_start_chase(uint32_t interval_ms);
esp_err_t rgb_led_stop_chase(void);
