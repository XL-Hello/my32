/**
 * @file lv_port_disp.h
 * @brief LVGL 显示端口适配接口。
 */

/* 将此文件复制为 "lv_port_disp.h" 后，把此值设为 "1" 以启用内容。 */
#if 1

#ifndef LV_PORT_DISP_TEMPL_H
#define LV_PORT_DISP_TEMPL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      包含头文件
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      宏定义
 *********************/

/**********************
 *      类型定义
 **********************/

/**********************
 *    全局函数声明
 **********************/
/* 初始化底层显示驱动。 */
void lv_port_disp_init(void);

/* 允许 LVGL 调用 disp_flush() 时更新屏幕。 */
void disp_enable_update(void);

/* 禁止 LVGL 调用 disp_flush() 时更新屏幕。 */
void disp_disable_update(void);

/**
 * @brief 获取最近一秒内完成的 LVGL 刷新周期数。
 *
 * 统计单位为一次脏矩形刷新周期，而不是单个 DMA 传输块；超过一秒无刷新时返回 0。
 */
uint32_t lv_port_disp_get_refresh_fps(void);

/**********************
 *        宏
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_PORT_DISP_TEMPL_H */

#endif /* 禁用或启用文件内容。 */
