#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/** 触摸控制器使用的独立 SPI 片选，低有效。 */
#define TOUCH_PIN_CS 36

/** 触摸控制器的按下中断输入，通常低有效。 */
#define TOUCH_PIN_IRQ 35

/** HR2046/XPT2046 兼容控制器的常用 12-bit 坐标读取命令。 */
#define TOUCH_CMD_READ_X 0xD0
#define TOUCH_CMD_READ_Y 0x90

typedef struct {
    uint16_t raw_x;
    uint16_t raw_y;
    bool pressed;
    bool valid;
} touch_raw_data_t;

/**
 * @brief 初始化 HR2046 触摸设备并启动 raw 坐标测试日志任务。
 *
 * @note 必须在 lcd_init() 成功后调用；LCD 组件已初始化 SPI2 总线。
 */
esp_err_t touch_init(void);

/**
 * @brief 读取一次未经校准的 HR2046 原始坐标。
 *
 * @param[out] data 按下状态、坐标有效性与 12-bit 原始 X/Y 坐标。未按下或坐标无效时坐标为 0。
 */
esp_err_t touch_read_raw(touch_raw_data_t *data);
