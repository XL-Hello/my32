#pragma once

#include <stdint.h>

#include "esp_err.h"

#define LCD_H_RES 240
#define LCD_V_RES 320

#define LCD_PIN_SCLK 12
#define LCD_PIN_MOSI 11
#define LCD_PIN_MISO 13
#define LCD_PIN_CS 7
#define LCD_PIN_DC 9
#define LCD_PIN_RST 8

/**
 * @brief 初始化 SPI 总线和 ILI9341 LCD。
 */
esp_err_t lcd_init(void);

/**
 * @brief 将 LCD 填充为指定的 RGB888 颜色。
 */
esp_err_t lcd_fill(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 启动循环显示红、绿、蓝、白、黑测试色的任务。
 */
esp_err_t lcd_test_colors(void);

/**
 * @brief 启动循环读取 ILI9341 ID 寄存器并打印读取结果的任务。
 */
esp_err_t lcd_test_version(void);
