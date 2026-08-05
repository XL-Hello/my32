/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "lcd.h"
#include "rgb_led.h"
#include "ws2812b.h"
#include "lvgl_port.h"
#include "home_ui.h"
#include "environment_sensor.h"
#include "cpu_usage.h"
#include "system_time.h"
#include "w25q64_test.h"
#include "littlefs_esp.h"
#include "littlefs_test.h"
#include "wifi_manager.h"
#include "ui_font.h"
#include "system_monitor_ui.h"

#define LOG_TAG "main"
#include "platform_log.h"

/**************************************功能配置与说明区******************************************/
#define ENABLE_ILI9341_LCD              1  // [ILI9341 LCD+彩色液晶显示屏][使用 ESP-IDF SPI2 驱动，40 MHz][GPIO8(MISO)、GPIO3(SCLK)、GPIO46(MOSI)、GPIO9(DC)、GPIO10(RST)、GPIO11(CS)]
#define ENABLE_LVGL                     1  // [LVGL+图形用户界面库][通过 LVGL 显示适配层调用 LCD SPI2 驱动；HR2046 触摸使用独立 SPI3]
#define ENABLE_CPU_FPS                  1  // [CPU 使用率与帧率监控][使用 FreeRTOS 任务状态 API 与 LVGL Tick Hook][占用少量 CPU 时间片]
#define ENABLE_GLOBAL_PERFORMANCE_MONITOR 0  // [全局性能悬浮窗][调试时设为 1，显示 CPU0/CPU1 与 FPS]

#define ENABLE_WS2812B_LED              1  // [WS2812B LED+可编程彩灯][使用 RMT 组件驱动][占用 GPIO48]
#define ENABLE_RGB_LED                  1  // [RGB LED+三色指示灯][使用 LEDC 组件 PWM 驱动][占用 GPIO12、GPIO13、GPIO14]
#define ENABLE_ENVIRONMENT_SENSOR       1  // [AHT20+温湿度传感器][使用 ESP-IDF I2C 驱动][占用 I2C0、GPIO2(SDA)、GPIO1(SCL)]

#define ENABLE_W25Q64                   1  // [W25Q64+SPI NOR Flash][使用软件 SPI 驱动][占用 GPIO36(SCLK)、GPIO35(MOSI)、GPIO37(MISO)、GPIO38(CS) 及一个 GPTimer]
#define ENABLE_SIM_UART                 1  // [软件 UART+串行通信接口][使用 GPIO 中断与 GPTimer 模拟驱动][占用 GPIO9(TX)、GPIO11(RX) 及两个 GPTimer]
#define ENABLE_SIM_SPI                  1  // [软件 SPI+串行外设接口][使用 GPIO 与 GPTimer 模拟驱动][占用 GPIO36(SCLK)、GPIO35(MOSI)、GPIO37(MISO)、GPIO38(CS) 及一个 GPTimer（与 W25Q64 共用）]
#define ENABLE_LITTLEFS                 1  // [LittleFS 文件系统][使用 ESP32-S3 主 Flash 尾部 2 MiB 分区，挂载路径 /littlefs]
/*********************************************************************************************/


/**************************************自己的工具说明******************************************/
#define ENABLE_RINGBUFFER                 1
#define ENABLE_LOG                        1

#if ENABLE_LITTLEFS
static const littlefs_esp_config_t s_littlefs_config = {
    .base_path = "/littlefs",
    .partition_label = "littlefs",
    .format_if_mount_failed = false,
};
#endif

static void app_ui_init(void)
{
    if (!ui_font_init()) {
        log_error("UI font initialization failed");
        return;
    }

    home_ui_create();
#if ENABLE_GLOBAL_PERFORMANCE_MONITOR
    system_monitor_ui_create();
#endif
}

void my_main()
{
    // vTaskDelay(pdMS_TO_TICKS(3000));

#if ENABLE_LITTLEFS
    ESP_ERROR_CHECK(littlefs_esp_mount(&s_littlefs_config));
    //ESP_ERROR_CHECK(littlefs_esp_test());
    //ESP_ERROR_CHECK(littlefs_esp_speed_test());
#endif

    ESP_ERROR_CHECK(wifi_manager_init());
    log_info("Wi-Fi manager initialized");

    // W25Q64 自检完成后会释放软件 SPI 使用的 GPTimer。
    esp_err_t w25q64_result = w25q64_communication_test();
    if (w25q64_result != ESP_OK) {
        log_error("W25Q64 communication test failed: %s", esp_err_to_name(w25q64_result));
    }

    // ILI9341 LCD
    // ESP_ERROR_CHECK(lcd_init());
    // ESP_ERROR_CHECK(lcd_test_colors());
    //ESP_ERROR_CHECK(lcd_test_version());
    ESP_ERROR_CHECK(environment_sensor_init());
    log_info("environment sensor service started");

    log_info("LVGL initialization started");
    lvgl_port_init(app_ui_init);
    log_info("LVGL tick timer and handler task started");

    ESP_ERROR_CHECK(rgb_led_init());
    ESP_ERROR_CHECK(rgb_led_start_chase(2000));
    log_info("RGB LED initialized");

    // ws2812b
    ws2812b_init();
    ws2812b_set_pixel(0, 5, 5, 5);
    ws2812b_refresh();
    log_info("ws2812b initialized");

}

void app_main(void)
{
    printf("Hello world!\n");

    ESP_ERROR_CHECK(system_time_init());
    ESP_ERROR_CHECK(cpu_usage_init());

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
