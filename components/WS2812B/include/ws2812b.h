#pragma once

#include <stdint.h>

#include "esp_err.h"

/* 后端选择值。编译时通过 WS2812B_BACKEND 宏选择其中之一。 */
#define WS2812B_BACKEND_RMT 1
#define WS2812B_BACKEND_SPI 2

/* 默认使用 RMT；可在组件编译定义中覆盖为 WS2812B_BACKEND_SPI。 */
#ifndef WS2812B_BACKEND
#define WS2812B_BACKEND WS2812B_BACKEND_RMT
#endif

/* 默认硬件参数；可在组件编译定义中覆盖。 */
#ifndef WS2812B_GPIO_NUM
#define WS2812B_GPIO_NUM 48
#endif

#ifndef WS2812B_LED_COUNT
#define WS2812B_LED_COUNT 1U
#endif

#ifndef WS2812B_RMT_RESOLUTION_HZ
#define WS2812B_RMT_RESOLUTION_HZ (10U * 1000U * 1000U)
#endif

/**
 * @brief 初始化 WS2812B 灯带。
 *
 * 初始化后所有灯珠均熄灭。重复调用直接返回 ESP_OK。
 */
esp_err_t ws2812b_init(void);

/**
 * @brief 在内部帧缓存中设置一个像素的 RGB 值。
 *
 * 调用本函数后需调用 ws2812b_refresh() 才会输出到灯带。
 */
esp_err_t ws2812b_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 设置全部像素并立即刷新到灯带。
 */
esp_err_t ws2812b_set_all(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 将内部帧缓存刷新到灯带。
 */
esp_err_t ws2812b_refresh(void);

/**
 * @brief 熄灭所有像素并立即刷新。
 */
esp_err_t ws2812b_clear(void);

/**
 * @brief 释放所选后端分配的资源。
 */
esp_err_t ws2812b_deinit(void);
