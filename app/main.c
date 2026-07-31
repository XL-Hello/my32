/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "lcd.h"
#include "rgb_led.h"
#include "ws2812b.h"
#include "display/lvgl_adapt/lvgl_port.h"
#include "services/environment_sensor/environment_sensor.h"
#include "services/cpu_usage/cpu_usage.h"
#include "services/system_time/system_time.h"
#include "sim_spi.h"
#include "w25q64.h"

#define LOG_TAG "main"
#include "platform_log.h"

#define W25Q64_TEST_ADDRESS (W25Q64_CAPACITY_BYTES - W25Q64_SECTOR_SIZE)

/**************************************功能配置与说明区******************************************/
#define ENABLE_ILI9341_LCD              1  // [ILI9341 LCD+彩色液晶显示屏][使用 ESP-IDF SPI2 驱动][占用 SPI2_HOST、GPIO5(MISO)、GPIO6(SCLK)、GPIO7(MOSI)、GPIO15(DC)、GPIO16(RST)、GPIO17(CS)]
#define ENABLE_LVGL                     1  // [LVGL+图形用户界面库][通过 LVGL 显示适配层调用 LCD 驱动][复用 ILI9341 LCD 的 SPI2 与 GPIO 资源，不额外占用硬件资源]
#define ENABLE_CPU_FPS                  1  // [CPU 使用率与帧率监控][使用 FreeRTOS 任务状态 API 与 LVGL Tick Hook][占用少量 CPU 时间片]

#define ENABLE_WS2812B_LED              1  // [WS2812B LED+可编程彩灯][使用 RMT 组件驱动][占用 GPIO48]
#define ENABLE_RGB_LED                  1  // [RGB LED+三色指示灯][使用 LEDC 组件 PWM 驱动][占用 GPIO12、GPIO13、GPIO14]
#define ENABLE_ENVIRONMENT_SENSOR       1  // [AHT20+温湿度传感器][使用 ESP-IDF I2C 驱动][占用 I2C0、GPIO2(SDA)、GPIO1(SCL)]

#define ENABLE_W25Q64                   1  // [W25Q64+SPI NOR Flash][使用软件 SPI 驱动][占用 GPIO36(SCLK)、GPIO35(MOSI)、GPIO37(MISO)、GPIO38(CS) 及一个 GPTimer]
#define ENABLE_SIM_UART                 1  // [软件 UART+串行通信接口][使用 GPIO 中断与 GPTimer 模拟驱动][占用 GPIO9(TX)、GPIO11(RX) 及两个 GPTimer]
#define ENABLE_SIM_SPI                  1  // [软件 SPI+串行外设接口][使用 GPIO 与 GPTimer 模拟驱动][占用 GPIO36(SCLK)、GPIO35(MOSI)、GPIO37(MISO)、GPIO38(CS) 及一个 GPTimer（与 W25Q64 共用）]
/*********************************************************************************************/


/**************************************自己的工具说明******************************************/
#define ENABLE_RINGBUFFER                 1
#define ENABLE_LOG                        1



static sim_spi_config_t s_w25q64_spi;
static w25q64_t s_w25q64;

static esp_err_t w25q64_communication_test(void)
{
    static const uint8_t write_data[] = {
        0x53, 0x49, 0x4D, 0x2D, 0x53, 0x50, 0x49, 0x20,
        0x57, 0x32, 0x35, 0x51, 0x36, 0x34, 0x20, 0x4F,
        0x4B,
    };
    uint8_t read_data[sizeof(write_data)] = { 0 };
    uint32_t jedec_id = 0;

    esp_err_t result = sim_spi_init(&s_w25q64_spi);
    if (result != ESP_OK) {
        return result;
    }

    esp_err_t err = w25q64_init(&s_w25q64, &s_w25q64_spi);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_read_jedec_id(&s_w25q64, &jedec_id);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    log_info("W25Q64 JEDEC ID: 0x%06" PRIX32, jedec_id);
    if ((jedec_id & 0xFFU) != W25Q64_JEDEC_CAPACITY_CODE) {
        log_error("unexpected flash capacity code: 0x%02" PRIX32, jedec_id & 0xFFU);
        result = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }

    // 此测试会擦除最后一个 4 KiB 扇区 0x7FF000，请勿在该区域保存业务数据。
    err = w25q64_erase_sector(&s_w25q64, W25Q64_TEST_ADDRESS);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_write(&s_w25q64, W25Q64_TEST_ADDRESS, write_data, sizeof(write_data));
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_read(&s_w25q64, W25Q64_TEST_ADDRESS, read_data, sizeof(read_data));
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    if (memcmp(write_data, read_data, sizeof(write_data)) != 0) {
        result = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }

    log_info("W25Q64 write/read verification passed at 0x%06" PRIX32,
             (uint32_t)W25Q64_TEST_ADDRESS);
    result = ESP_OK;

cleanup:
    err = sim_spi_deinit(&s_w25q64_spi);
    if (result == ESP_OK && err != ESP_OK) {
        result = err;
    }
    return result;
}



void my_main()
{
    // vTaskDelay(pdMS_TO_TICKS(3000));

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
    lvgl_port_init();
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
