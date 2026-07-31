#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t rgb_led_driver_init(void);
esp_err_t rgb_led_driver_set(uint8_t red, uint8_t yellow, uint8_t green);
