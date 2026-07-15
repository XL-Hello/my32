/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "lcd.h"
#include "rgb_led.h"
#include "ws2812b.h"
#include "display/lvgl_adapt/lvgl_port.h"

#define LOG_TAG "main"
#include "platform_log.h"



void my_main()
{
    //vTaskDelay(pdMS_TO_TICKS(3000));

    // ILI9341 LCD
    // ESP_ERROR_CHECK(lcd_init());
    // ESP_ERROR_CHECK(lcd_test_colors());
    //ESP_ERROR_CHECK(lcd_test_version());
    log_info("LVGL initialization started");
    lvgl_port_init();
    log_info("LVGL tick timer and handler task started");

    // RGB LED
    ESP_ERROR_CHECK(rgb_led_init());
    ESP_ERROR_CHECK(rgb_led_start_chase(2000));
    log_info("RGB LED power-on self-test is running");

    // ws2812b
    ws2812b_init();
    ws2812b_set_pixel(0, 5, 5, 5);
    ws2812b_refresh();
    log_info("ws2812b initialized");



    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        //log_info("is running\n");
    }
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    my_main();
}
