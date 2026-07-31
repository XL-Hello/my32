/**
 * @file lv_port_disp.c
 * @brief LVGL 显示端口适配模板。
 */

/* 将此文件复制为 "lv_port_disp.c" 后，把此值设为 "1" 以启用内容。 */
#if 1

/*********************
 *      包含头文件
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "lcd.h"

/*********************
 *      宏定义
 *********************/
#define MY_DISP_HOR_RES LCD_H_RES
#define MY_DISP_VER_RES LCD_V_RES
#define LVGL_FPS_WINDOW_MS 1000

/**********************
 *      类型定义
 **********************/

/**********************
 *    静态函数声明
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
static bool lcd_flush_ready_callback(void *user_ctx);
static void disp_monitor_callback(lv_disp_drv_t *disp_drv, uint32_t time_ms,
                                  uint32_t pixel_count);
// static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                      const lv_area_t * fill_area, lv_color_t color);

/**********************
 *      静态变量
 **********************/
static lv_disp_drv_t *s_pending_disp_drv;
static uint32_t s_fps_window_start_ms;
static uint32_t s_last_refresh_ms;
static uint32_t s_refresh_count;
static uint32_t s_refresh_fps;

/**********************
 *        宏
 **********************/

/**********************
 *      全局函数
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     *     初始化显示硬件
     *-----------------------*/
    disp_init();

    /*-----------------------------
     *       创建 LVGL 绘图缓冲区
     *----------------------------*/

    /**
     * LVGL 需要使用此缓冲区在内存中绘制控件。
     * 绘制完成后，LVGL 会将缓冲区传给显示驱动的 `flush_cb`，由该回调将像素复制到屏幕。
     * 缓冲区至少要能容纳一行以上的像素。
     *
     * 可选的三种缓冲方案：
     * 1. 单缓冲：
     *    LVGL 在一个缓冲区中绘制，再将其内容发送到屏幕。
     *
     * 2. 双小缓冲：
     *    LVGL 在一个缓冲区绘制，并通过 DMA 将它发送到屏幕；发送期间，LVGL 可在另一个缓冲区
     *    绘制下一块区域，从而让绘制和传输并行。
     *
     * 3. 双全屏缓冲：
     *    设置两个全屏缓冲区，并将 `disp_drv.full_refresh` 设为 1。此时 `flush_cb` 每次都会收到
     *    完整屏幕的像素，只需切换帧缓冲区地址。
     */

    /* 方案 1：单缓冲。 */
    // static lv_disp_draw_buf_t draw_buf_dsc_1;
    // static lv_color_t buf_1[MY_DISP_HOR_RES * 20]; /* 可容纳 20 行像素的缓冲区。 */
    // lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 20); /* 初始化显示缓冲区。 */

    // /* 方案 2：双小缓冲。 */
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 10]; /* 可容纳 10 行像素的第一个缓冲区。 */
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 10]; /* 可容纳 10 行像素的第二个缓冲区。 */
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 10); /* 初始化显示缓冲区。 */

    // /* 方案 3：双全屏缓冲；还需在下方设置 disp_drv.full_refresh = 1。 */
    // static lv_disp_draw_buf_t draw_buf_dsc_3;
    // static lv_color_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES]; /* 一个全屏缓冲区。 */
    // static lv_color_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES]; /* 另一个全屏缓冲区。 */
    // lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
    //                       MY_DISP_VER_RES * LV_VER_RES_MAX); /* 初始化显示缓冲区。 */

    /*-----------------------------------
     *       向 LVGL 注册显示驱动
     *----------------------------------*/

    static lv_disp_drv_t disp_drv; /* 显示驱动描述符。 */
    lv_disp_drv_init(&disp_drv); /* 基础初始化。 */

    /* 配置访问实际显示屏所需的参数。 */

    /* 设置显示屏分辨率。 */
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;

    /* 设置将绘图缓冲区内容发送到显示屏的回调。 */
    disp_drv.flush_cb = disp_flush;

    /* 每个 LVGL 脏矩形刷新周期完成时调用一次，用于统计实际 UI 刷新率。 */
    disp_drv.monitor_cb = disp_monitor_callback;

    /* 绑定显示缓冲区。 */
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /* 方案 3 必需的设置。 */
    // disp_drv.full_refresh = 1;

    /* 如果 MCU 具有图形加速器，可用它填充内存数组。
     * `lv_conf.h` 可启用 LVGL 内建支持的 GPU；若使用其他 GPU，也可通过此回调接入。 */
    // disp_drv.gpu_fill_cb = gpu_fill;

    /* 最后注册驱动。 */
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *      静态函数
 **********************/

/* 初始化显示屏及其必需外设。 */
static void disp_init(void)
{
    /* 在此实现显示硬件初始化。 */
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(lcd_set_color_trans_done_callback(lcd_flush_ready_callback, NULL));
    ESP_ERROR_CHECK(lcd_set_orientation(LCD_ORIENTATION_PORTRAIT_INVERTED));
}

volatile bool disp_flush_enabled = true;

/* 允许 LVGL 调用 disp_flush() 时更新屏幕。 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* 禁止 LVGL 调用 disp_flush() 时更新屏幕。 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

uint32_t lv_port_disp_get_refresh_fps(void)
{
    if (s_last_refresh_ms == 0 || lv_tick_elaps(s_last_refresh_ms) >= LVGL_FPS_WINDOW_MS) {
        return 0;
    }
    return s_refresh_fps;
}

/* 将内部绘图缓冲区中指定区域的内容刷新到屏幕。
 * 可以使用 DMA 或硬件加速器在后台执行；传输完成后必须调用 `lv_disp_flush_ready()`。 */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if (!disp_flush_enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    /* LVGL 的 x2/y2 是包含边界；esp_lcd 的结束坐标不包含。 */
    s_pending_disp_drv = disp_drv;
    ESP_ERROR_CHECK(lcd_draw_bitmap(area->x1, area->y1,
                                    area->x2+1, area->y2+1, color_p));

    /* 传输由 SPI DMA 异步执行；完成后由 lcd_flush_ready_callback() 通知 LVGL。 */
}

static bool lcd_flush_ready_callback(void *user_ctx)
{
    (void)user_ctx;

    if (s_pending_disp_drv != NULL) {
        lv_disp_flush_ready(s_pending_disp_drv);
        s_pending_disp_drv = NULL;
    }

    return false;
}

/*
 * monitor_cb 对应一次 LVGL 刷新周期，而非一次 disp_flush()。
 * 一个周期可以包含多个脏矩形及多个 DMA 传输块，因而适合作为 UI FPS 的计数单位。
 */
static void disp_monitor_callback(lv_disp_drv_t *disp_drv, uint32_t time_ms,
                                  uint32_t pixel_count)
{
    (void)disp_drv;
    (void)time_ms;

    if (pixel_count == 0) {
        return;
    }

    const uint32_t now_ms = lv_tick_get();
    if (s_fps_window_start_ms == 0) {
        s_fps_window_start_ms = now_ms;
    }

    s_last_refresh_ms = now_ms;
    ++s_refresh_count;

    const uint32_t elapsed_ms = lv_tick_elaps(s_fps_window_start_ms);
    if (elapsed_ms >= LVGL_FPS_WINDOW_MS) {
        s_refresh_fps = s_refresh_count * 1000U / elapsed_ms;
        s_refresh_count = 0;
        s_fps_window_start_ms = now_ms;
    }
}

/* 可选：GPU 接口。 */

/* 如果 MCU 具有硬件加速器（GPU），可用它将某个颜色填充到内存中。 */
// static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                      const lv_area_t * fill_area, lv_color_t color)
// {
//     /* 这是需要由实际 GPU 实现的示例代码。 */
//     int32_t x, y;
//     dest_buf += dest_width * fill_area->y1; /* 移动到第一行。 */
//
//     for(y = fill_area->y1; y <= fill_area->y2; y++) {
//         for(x = fill_area->x1; x <= fill_area->x2; x++) {
//             dest_buf[x] = color;
//         }
//         dest_buf += dest_width; /* 移动到下一行。 */
//     }
// }

#else /* 在文件顶部启用此文件。 */

/* 此虚拟 typedef 仅用于消除 -Wpedantic 警告。 */
typedef int keep_pedantic_happy;
#endif
