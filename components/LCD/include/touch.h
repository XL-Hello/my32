#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/** 触摸控制器使用的独立 SPI 片选，低有效。 */
#define TOUCH_PIN_CS 8

/** 触摸控制器的按下状态输入，通常低有效；由 LVGL 输入回调轮询。 */
#define TOUCH_PIN_IRQ 18

/** HR2046/XPT2046 兼容控制器的常用 12-bit 坐标读取命令。 */
#define TOUCH_CMD_READ_X 0xD0
#define TOUCH_CMD_READ_Y 0x90

typedef struct {
    uint16_t raw_x;
    uint16_t raw_y;
    bool pressed;
    bool valid;
} touch_raw_data_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
    bool valid;
} touch_point_t;

/**
 * @brief 初始化 HR2046 触摸设备。
 *
 * @note 必须在 lcd_init() 成功后调用；LCD 组件已初始化 SPI2 总线。
 *       坐标由调用方按需通过 touch_read_point() 读取；默认不创建原始坐标日志任务。
 */
esp_err_t touch_init(void);

/**
 * @brief 读取一次未经校准的 HR2046 原始坐标。
 *
 * @param[out] data 按下状态、坐标有效性与 12-bit 原始 X/Y 坐标。未按下或坐标无效时坐标为 0。
 */
esp_err_t touch_read_raw(touch_raw_data_t *data);

/**
 * @brief 读取一次已校准的屏幕像素坐标。
 *
 * 坐标基于当前 240x320 的 LCD_ORIENTATION_PORTRAIT_INVERTED 显示方向，
 * 有效坐标范围为 x=0..239、y=0..319。
 *
 * @param[out] data 按下状态、坐标有效性与已校准的屏幕像素坐标。未按下或
 *                  坐标无效时坐标为 0。
 */
esp_err_t touch_read_point(touch_point_t *data);
