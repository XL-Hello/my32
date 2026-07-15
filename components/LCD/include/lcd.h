#pragma once

#include <stdbool.h>
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
 * @brief LCD 颜色 DMA 传输完成回调。
 *
 * 回调运行在 SPI ISR 上下文，只能执行不会阻塞的操作。
 * 返回 true 表示需要在 ISR 退出后请求调度。
 */
typedef bool (*lcd_color_trans_done_cb_t)(void *user_ctx);

/**
 * @brief 初始化 SPI 总线和 ILI9341 LCD。
 */
esp_err_t lcd_init(void);

/**
 * @brief 将一块 RGB565 像素数据提交到 LCD。
 *
 * @note 传输由 SPI DMA 在后台完成。调用方在 LCD 组件通知传输完成前，
 *       不可复用 @p color_data 指向的缓冲区。
 *
 * @param x_start 矩形左边界（包含）。
 * @param y_start 矩形上边界（包含）。
 * @param x_end 矩形右边界（不包含）。
 * @param y_end 矩形下边界（不包含）。
 * @param color_data RGB565 像素数据。
 * @return ESP_OK 表示传输已成功提交。
 */
esp_err_t lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end,
                          const void *color_data);

/**
 * @brief 设置颜色 DMA 传输完成回调。
 *
 * @note 调用方必须在首次调用 lcd_draw_bitmap() 前完成设置。LVGL 运行期间不能同时运行
 *       lcd_test_colors()，两者会共享同一个 LCD 面板和完成回调。
 */
esp_err_t lcd_set_color_trans_done_callback(lcd_color_trans_done_cb_t callback,
                                            void *user_ctx);

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
