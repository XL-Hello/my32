/**
 * @file lv_conf.h
 * v8.4.0 配置文件
 */

/*
 * 将此文件复制为 `lv_conf.h`
 * 1. 直接放在 `lvgl` 文件夹旁
 * 2. 或放在其他位置，并且
 *    - 定义 `LV_CONF_INCLUDE_SIMPLE`
 *    - 将路径加入包含路径
 */

/* clang-format off */
#if 1 /*设为“1”以启用内容*/

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   颜色设置
 *====================*/

/*颜色深度：1（每像素 1 字节）、8（RGB332）、16（RGB565）、32（ARGB8888）*/
#define LV_COLOR_DEPTH 16

/*交换 RGB565 颜色的两个字节。显示屏使用 8 位接口（如 SPI）时很有用。*/
#define LV_COLOR_16_SWAP 1

/*启用在透明背景上绘制的功能。
 *使用 opa 和 transform_* 样式属性时必须启用。
 *当 UI 位于其他图层（如 OSD 菜单或视频播放器）之上时也可使用。*/
#define LV_COLOR_SCREEN_TRANSP 0

/*调整颜色混合函数的舍入方式。不同 GPU 的颜色混合（混色）计算可能不同。
 * 0：向下舍入，64：从 x.75 向上舍入，128：从一半向上舍入，192：从 x.25 向上舍入，254：向上舍入。*/
#define LV_COLOR_MIX_ROUND_OFS 0

/*使用色度键时，具有此颜色的图像像素不会被绘制。*/
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)         /*纯绿色*/

/*=========================
   内存设置
 *=========================*/

/*
 * LVGL 对象、文本和图片解码的动态内存固定分配到 PSRAM；显示 DMA 双缓冲仍使用
 * 内部 DRAM，避免 LCD 刷新受外部 RAM 带宽和缓存访问影响。
 */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 0
    /*可供 `lv_mem_alloc()` 使用的内存大小，单位为字节（>= 2 kB）。*/
    #define LV_MEM_SIZE (48U * 1024U)          /*[字节]*/

    /*为内存池设置地址，而非将其分配为普通数组；也可位于外部 SRAM。*/
    #define LV_MEM_ADR 0     /*0：未使用*/
    /*也可不提供地址，而是提供一个为 LVGL 获取内存池的内存分配器，例如 my_malloc。*/
    #if LV_MEM_ADR == 0
        #undef LV_MEM_POOL_INCLUDE
        #undef LV_MEM_POOL_ALLOC
#endif

#else       /*LV_MEM_CUSTOM*/
    #define LV_MEM_CUSTOM_INCLUDE "platform.h"
    #define LV_MEM_CUSTOM_ALLOC(size) ps_malloc(size)
    #define LV_MEM_CUSTOM_FREE(ptr) free(ptr)
    #define LV_MEM_CUSTOM_REALLOC(ptr, size) ps_realloc(ptr, size)
#endif     /*LV_MEM_CUSTOM*/

/*渲染和其他内部处理机制使用的中间内存缓冲区数量。
 *缓冲区数量不足时会输出错误日志。*/
#define LV_MEM_BUF_MAX_NUM 16

/*使用标准 `memcpy` 和 `memset` 替代 LVGL 自有函数（不一定更快）。*/
#define LV_MEMCPY_MEMSET_STD 0

/*====================
   HAL 设置
 *====================*/

/*默认显示刷新周期。LVGL 将按此周期重绘发生变化的区域。*/
#define LV_DISP_DEF_REFR_PERIOD 30      /*[毫秒]*/

/*输入设备读取周期，单位为毫秒。*/
#define LV_INDEV_DEF_READ_PERIOD 30     /*[毫秒]*/

/*使用可提供已过去毫秒数的自定义 tick 源。
 *这样无需通过 `lv_tick_inc()` 手动更新 tick。*/
#define LV_TICK_CUSTOM 0
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"         /*系统时间函数的头文件*/
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())    /*计算当前系统时间（毫秒）的表达式*/
    /*如果将 lvgl 用作 ESP32 组件*/
    // #define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
    // #define LV_TICK_CUSTOM_SYS_TIME_EXPR ((esp_timer_get_time() / 1000LL))
#endif   /*LV_TICK_CUSTOM*/

/*默认每英寸点数。用于初始化默认尺寸，例如控件尺寸和样式内边距。
 *（并不十分重要，可调整它以修改默认尺寸和间距。）*/
#define LV_DPI_DEF 130     /*[像素/英寸]*/

/*=======================
 * 功能配置
 *=======================*/

/*-------------
 * 绘制
 *-----------*/

/*启用复杂绘制引擎。
 *绘制阴影、渐变、圆角、圆、弧、斜线、图像变换或任何蒙版时需要启用。*/
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0

    /*允许缓存部分阴影计算结果。
    *LV_SHADOW_CACHE_SIZE 是可缓存的最大阴影尺寸，其中阴影尺寸为 `shadow_width + radius`。
    *缓存会消耗 LV_SHADOW_CACHE_SIZE^2 的 RAM。*/
    #define LV_SHADOW_CACHE_SIZE 0

    /*设置最多缓存的圆形数据数量。
    *为抗锯齿保存 1/4 圆的周长数据。
    *每个圆消耗 radius * 4 字节（保存最常用的半径）。
    *0：禁用缓存。*/
    #define LV_CIRCLE_CACHE_SIZE 4
#endif /*LV_DRAW_COMPLEX*/

/**
 * 当控件的 `style_opa < 255` 时，将使用“简单图层”把控件缓冲到图层中，
 * 并以给定不透明度将其作为图像混合。
 * 注意，`bg_opa`、`text_opa` 等无需缓冲到图层中。
 * 控件可拆分为较小的块进行缓冲，以避免使用大型缓冲区。
 *
 * - LV_LAYER_SIMPLE_BUF_SIZE：[字节] 最佳目标缓冲区大小，LVGL 将尝试分配它。
 * - LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE：[字节] 无法分配 `LV_LAYER_SIMPLE_BUF_SIZE` 时使用。
 *
 * 两种缓冲区大小均以字节为单位。
 * “变换图层”（使用 transform_angle/zoom 属性）需要更大的缓冲区，
 * 且无法分块绘制，因此这些设置仅影响带不透明度的控件。
 */
#define LV_LAYER_SIMPLE_BUF_SIZE          (24 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (3 * 1024)

/*默认图像缓存大小。图像缓存会保持图像打开状态。
 *若仅使用内置图像格式，缓存没有实际优势（即未添加新的图像解码器）。
 *使用复杂图像解码器（如 PNG 或 JPG）时，缓存可避免反复打开和解码图像。
 *但已打开的图像可能占用额外 RAM。
 *0：禁用缓存。*/
#define LV_IMG_CACHE_DEF_SIZE 0

/*每个渐变允许的色标数量。增大此值可允许更多色标。
 *每增加一个色标会增加 (sizeof(lv_color_t) + 1) 字节。*/
#define LV_GRADIENT_MAX_STOPS 2

/*默认渐变缓冲区大小。
 *LVGL 计算渐变“映射”时可将其保存到缓存中，避免重复计算。
 *LV_GRAD_CACHE_DEF_SIZE 以字节为单位设置该缓存的大小。
 *若缓存过小，映射仅在绘制需要时分配。
 *0 表示不缓存。*/
#define LV_GRAD_CACHE_DEF_SIZE 0

/*允许对渐变进行抖动处理（以在有限色深显示屏上获得视觉平滑的颜色渐变）。
 *LV_DITHER_GRADIENT 表示需额外分配对象渲染表面的一行或两行。
 *内存增加量为（32 位 * 对象宽度）；使用误差扩散时还会增加 24 位 * 对象宽度。*/
#define LV_DITHER_GRADIENT 0
#if LV_DITHER_GRADIENT
    /*添加对误差扩散抖动的支持。
     *误差扩散抖动具有更好的视觉效果，但绘制时会消耗更多 CPU 和内存。
     *内存增加量为（24 位 * 对象宽度）。*/
    #define LV_DITHER_ERROR_DIFFUSION 0
#endif

/*旋转操作可分配的最大缓冲区大小。
 *仅当显示驱动中启用了软件旋转时使用。*/
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*-------------
 * GPU
 *-----------*/

/*使用 Arm 的二维加速库 Arm-2D。*/
#define LV_USE_GPU_ARM2D 0

/*使用 STM32 的 DMA2D（又称 Chrom Art）GPU。*/
#define LV_USE_GPU_STM32_DMA2D 0
#if LV_USE_GPU_STM32_DMA2D
    /*必须定义为目标处理器 CMSIS 头文件的包含路径，
    例如 "stm32f7xx.h" 或 "stm32f4xx.h"。*/
    #define LV_GPU_DMA2D_CMSIS_INCLUDE
#endif

/*启用 RA6M3 G2D GPU。*/
#define LV_USE_GPU_RA6M3_G2D 0
#if LV_USE_GPU_RA6M3_G2D
    /*目标处理器的包含路径，
    例如 "hal_data.h"。*/
    #define LV_GPU_RA6M3_G2D_INCLUDE "hal_data.h"
#endif

/*使用 SWM341 的 DMA2D GPU。*/
#define LV_USE_GPU_SWM341_DMA2D 0
#if LV_USE_GPU_SWM341_DMA2D
    #define LV_GPU_SWM341_DMA2D_INCLUDE "SWM341.h"
#endif

/*在 NXP iMX RTxxx 平台上使用 PXP GPU。*/
#define LV_USE_GPU_NXP_PXP 0
#if LV_USE_GPU_NXP_PXP
    /*1：为 PXP 添加默认的裸机和 FreeRTOS 中断处理例程（lv_gpu_nxp_pxp_osa.c），
    *   并在 lv_init() 期间自动调用 lv_gpu_nxp_pxp_init()。使用 FreeRTOS OSA 时必须定义
    *   符号 SDK_OS_FREE_RTOS，否则将选择裸机实现。
    *0：必须在 lv_init() 前手动调用 lv_gpu_nxp_pxp_init()。
    */
    #define LV_USE_GPU_NXP_PXP_AUTO_INIT 0
#endif

/*在 NXP iMX RTxxx 平台上使用 VG-Lite GPU。*/
#define LV_USE_GPU_NXP_VG_LITE 0

/*使用 SDL 渲染器 API。*/
#define LV_USE_GPU_SDL 0
#if LV_USE_GPU_SDL
    #define LV_GPU_SDL_INCLUDE_PATH <SDL2/SDL.h>
    /*纹理缓存大小，默认 8 MB。*/
    #define LV_GPU_SDL_LRU_SIZE (1024 * 1024 * 8)
    /*用于蒙版绘制的自定义混合模式；如需链接较旧的 SDL2 库，请禁用。*/
    #define LV_GPU_SDL_CUSTOM_BLEND_MODE (SDL_VERSION_ATLEAST(2, 0, 6))
#endif

/*-------------
 * 日志
 *-----------*/

/*启用日志模块。*/
#define LV_USE_LOG 1
#if LV_USE_LOG

    /*日志记录级别：
    *LV_LOG_LEVEL_TRACE       大量日志，提供详细信息
    *LV_LOG_LEVEL_INFO        记录重要事件
    *LV_LOG_LEVEL_WARN        记录发生但未造成问题的不期望事件
    *LV_LOG_LEVEL_ERROR       仅记录可能导致系统失败的严重问题
    *LV_LOG_LEVEL_USER        仅记录用户添加的日志
    *LV_LOG_LEVEL_NONE        不记录任何日志*/
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /*1：使用 `printf` 输出日志；
    *0：用户需通过 `lv_log_register_print_cb()` 注册回调。*/
    #define LV_LOG_PRINTF 0

    /*在会产生大量日志的模块中启用/禁用 LV_LOG_TRACE。*/
    #define LV_LOG_TRACE_MEM        1
    #define LV_LOG_TRACE_TIMER      1
    #define LV_LOG_TRACE_INDEV      1
    #define LV_LOG_TRACE_DISP_REFR  1
    #define LV_LOG_TRACE_EVENT      1
    #define LV_LOG_TRACE_OBJ_CREATE 1
    #define LV_LOG_TRACE_LAYOUT     1
    #define LV_LOG_TRACE_ANIM       1

#endif  /*LV_USE_LOG*/

/*-------------
 * 断言
 *-----------*/

/*操作失败或发现无效数据时启用断言。
 *若启用 LV_USE_LOG，失败时将输出错误消息。*/
#define LV_USE_ASSERT_NULL          1   /*检查参数是否为 NULL。（非常快，推荐）*/
#define LV_USE_ASSERT_MALLOC        1   /*检查内存是否成功分配。（非常快，推荐）*/
#define LV_USE_ASSERT_STYLE         0   /*检查样式是否已正确初始化。（非常快，推荐）*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*关键操作后检查 `lv_mem` 的完整性。（较慢）*/
#define LV_USE_ASSERT_OBJ           0   /*检查对象类型和是否存在（如未被删除）。（较慢）*/

/*断言发生时添加自定义处理程序，例如重启 MCU。*/
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);   /*默认停止执行*/

/*-------------
 * 其他
 *-----------*/

/*1：显示 CPU 使用率和 FPS 计数。*/
#define LV_USE_PERF_MONITOR 1
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
#endif

/*1：显示已用内存和内存碎片情况。
 *需要 LV_MEM_CUSTOM = 0。*/
#define LV_USE_MEM_MONITOR 0
#if LV_USE_MEM_MONITOR
    #define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT
#endif

/*1：在重绘区域上绘制随机颜色的矩形。*/
#define LV_USE_REFR_DEBUG 0

/*替换内置的 (v)snprintf 函数。*/
#define LV_SPRINTF_CUSTOM 0
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
#else   /*LV_SPRINTF_CUSTOM*/
    #define LV_SPRINTF_USE_FLOAT 0
#endif  /*LV_SPRINTF_CUSTOM*/

#define LV_USE_USER_DATA 1

/*垃圾回收器设置。
 *当 lvgl 绑定到由该语言管理内存的高级语言时使用。*/
#define LV_ENABLE_GC 0
#if LV_ENABLE_GC != 0
    #define LV_GC_INCLUDE "gc.h"                           /*包含垃圾回收器相关内容*/
#endif /*LV_ENABLE_GC*/

/*=====================
 *  编译器设置
 *====================*/

/*大端系统设为 1。*/
#define LV_BIG_ENDIAN_SYSTEM 0

/*为 `lv_tick_inc` 函数定义自定义属性。*/
#define LV_ATTRIBUTE_TICK_INC

/*为 `lv_timer_handler` 函数定义自定义属性。*/
#define LV_ATTRIBUTE_TIMER_HANDLER

/*为 `lv_disp_flush_ready` 函数定义自定义属性。*/
#define LV_ATTRIBUTE_FLUSH_READY

/*缓冲区所需的对齐大小。*/
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/*将添加到需要内存对齐的位置（使用 -Os 时，数据默认可能不会边界对齐）。
 *例如：__attribute__((aligned(4)))。*/
#define LV_ATTRIBUTE_MEM_ALIGN

/*用于标记大型常量数组的属性，例如字体位图。*/
#define LV_ATTRIBUTE_LARGE_CONST

/*在 RAM 中声明大型数组时使用的编译器前缀。*/
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/*将性能关键函数放入更快的内存中（如 RAM）。*/
#define LV_ATTRIBUTE_FAST_MEM

/*为 GPU 加速操作中使用的变量添加前缀；这些变量通常需要放在 DMA 可访问的 RAM 段中。*/
#define LV_ATTRIBUTE_DMA

/*将整数常量导出至绑定层。该宏用于 LV_<CONST> 形式的常量，
 *它们也应出现在 MicroPython 等 LVGL 绑定 API 中。*/
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning /*默认值仅用于防止 GCC 警告*/

/*通过使用 int32_t 而非 int16_t 存储坐标，将默认 -32k..32k 坐标范围扩展到 -4M..4M。*/
#define LV_USE_LARGE_COORD 0

/*==================
 *   字体使用
 *===================*/

/*使用 bpp = 4 的 Montserrat 字体，包含 ASCII 范围和部分符号。
 *https://fonts.google.com/specimen/Montserrat*/
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

/*演示特殊功能。*/
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /*bpp = 3*/
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /*希伯来语、阿拉伯语、波斯语字母及其所有字形*/
#define LV_FONT_SIMSUN_16_CJK            0  /*1000 个最常用的 CJK 部首*/

/*像素级精确的等宽字体。*/
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/*文件字体在 LittleFS 挂载后运行时加载，不能在此声明为静态自定义字体。*/
#define LV_FONT_CUSTOM_DECLARE

/*运行时字体加载前，使用内置字体作为 LVGL 的安全默认值。*/
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*启用对大型字体和/或包含大量字符的字体的处理。
 *限制取决于字体大小、字体字形和 bpp。
 *字体需要此功能时会触发编译器错误。*/
#define LV_FONT_FMT_TXT_LARGE 0

/*启用/禁用对压缩字体的支持。*/
#define LV_USE_FONT_COMPRESSED 1

/*启用子像素渲染。*/
#define LV_USE_FONT_SUBPX 0
#if LV_USE_FONT_SUBPX
    /*设置显示屏的像素顺序，即 RGB 通道的物理顺序；对“普通”字体无影响。*/
    #define LV_FONT_SUBPX_BGR 0  /*0：RGB；1：BGR 顺序*/
#endif

/*找不到字形 dsc 时启用占位符绘制。*/
#define LV_USE_FONT_PLACEHOLDER 1

/*=================
 *  文本设置
 *=================*/

/**
 * 为字符串选择字符编码。
 * IDE 或编辑器应使用相同的字符编码。
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*可在这些字符处断开（换行）文本。*/
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/*若单词至少达到此长度，将在“最合适”的位置断开。
 *设为 <= 0 可禁用。*/
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/*长单词断开前一行中应保留的最少字符数。
 *取决于 LV_TXT_LINE_BREAK_LONG_LEN。*/
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/*长单词断开后一行中应保留的最少字符数。
 *取决于 LV_TXT_LINE_BREAK_LONG_LEN。*/
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/*用于指示文本重新着色的控制字符。*/
#define LV_TXT_COLOR_CMD "#"

/*支持双向文本，允许混用从左到右和从右到左的文本。
 *方向将根据 Unicode 双向算法处理：
 *https://www.w3.org/International/articles/inline-bidi-markup/uba-basics*/
#define LV_USE_BIDI 0
#if LV_USE_BIDI
    /*设置默认方向。支持的值：
    *`LV_BASE_DIR_LTR` 从左到右
    *`LV_BASE_DIR_RTL` 从右到左
    *`LV_BASE_DIR_AUTO` 检测文本的基本方向*/
    #define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#endif

/*启用阿拉伯语/波斯语处理。
 *这些语言中的字符应根据其在文本中的位置替换为相应字形。*/
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  控件使用
 *================*/

/*控件文档：https://docs.lvgl.io/latest/en/html/widgets/index.html*/

#define LV_USE_ARC        1

#define LV_USE_BAR        1

#define LV_USE_BTN        1

#define LV_USE_BTNMATRIX  1

#define LV_USE_CANVAS     1

#define LV_USE_CHECKBOX   1

#define LV_USE_DROPDOWN   1   /*需要：lv_label*/

#define LV_USE_IMG        1   /*需要：lv_label*/

#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 1 /*启用标签文本选择。*/
    #define LV_LABEL_LONG_TXT_HINT 1  /*在标签中存储额外信息以加速超长文本绘制。*/
#endif

#define LV_USE_LINE       1

#define LV_USE_ROLLER     1   /*需要：lv_label*/
#if LV_USE_ROLLER
    #define LV_ROLLER_INF_PAGES 7 /*滚筒无限循环时的额外“页”数*/
#endif

#define LV_USE_SLIDER     1   /*需要：lv_bar*/

#define LV_USE_SWITCH     1

#define LV_USE_TEXTAREA   1   /*需要：lv_label*/
#if LV_USE_TEXTAREA != 0
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500    /*毫秒*/
#endif

#define LV_USE_TABLE      1

/*==================
 * 扩展组件
 *==================*/

/*-----------
 * 控件
 *----------*/
#define LV_USE_ANIMIMG    1

#define LV_USE_CALENDAR   1
#if LV_USE_CALENDAR
    #define LV_CALENDAR_WEEK_STARTS_MONDAY 0
    #if LV_CALENDAR_WEEK_STARTS_MONDAY
        #define LV_CALENDAR_DEFAULT_DAY_NAMES {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"}
    #else
        #define LV_CALENDAR_DEFAULT_DAY_NAMES {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}
    #endif

    #define LV_CALENDAR_DEFAULT_MONTH_NAMES {"January", "February", "March",  "April", "May",  "June", "July", "August", "September", "October", "November", "December"}
    #define LV_USE_CALENDAR_HEADER_ARROW 1
    #define LV_USE_CALENDAR_HEADER_DROPDOWN 1
#endif  /*LV_USE_CALENDAR*/

#define LV_USE_CHART      1

#define LV_USE_COLORWHEEL 1

#define LV_USE_IMGBTN     1

#define LV_USE_KEYBOARD   1

#define LV_USE_LED        1

#define LV_USE_LIST       1

#define LV_USE_MENU       1

#define LV_USE_METER      1

#define LV_USE_MSGBOX     1

#define LV_USE_SPAN       1
#if LV_USE_SPAN
    /*一行文本可包含的 span 描述符最大数量。*/
    #define LV_SPAN_SNIPPET_STACK_SIZE 64
#endif

#define LV_USE_SPINBOX    1

#define LV_USE_SPINNER    1

#define LV_USE_TABVIEW    1

#define LV_USE_TILEVIEW   1

#define LV_USE_WIN        1

/*-----------
 * 主题
 *----------*/

/*简洁、美观且功能完整的主题。*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT

    /*0：浅色模式；1：深色模式*/
    #define LV_THEME_DEFAULT_DARK 0

    /*1：启用按下时放大*/
    #define LV_THEME_DEFAULT_GROW 1

    /*默认过渡时长，单位为 [毫秒]*/
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif /*LV_USE_THEME_DEFAULT*/

/*非常简洁的主题，适合作为自定义主题的起点。*/
#define LV_USE_THEME_BASIC 1

/*为单色显示屏设计的主题。*/
#define LV_USE_THEME_MONO 1

/*-----------
 * 布局
 *----------*/

/*类似 CSS 中 Flexbox 的布局。*/
#define LV_USE_FLEX 1

/*类似 CSS 中 Grid 的布局。*/
#define LV_USE_GRID 1

/*---------------------
 * 第三方库
 *--------------------*/

/*常见 API 的文件系统接口。*/

/*用于 fopen、fread 等的 API。*/
#define LV_USE_FS_STDIO 1
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER 'R'       /*LittleFS VFS 的资源盘符。*/
    #define LV_FS_STDIO_PATH ""         /*设置工作目录，文件/目录路径将附加到其后。*/
    #define LV_FS_STDIO_CACHE_SIZE 0    /*>0 时在 lv_fs_read() 中缓存相应字节数。*/
#endif

/*用于 open、read 等的 API。*/
#define LV_USE_FS_POSIX 0
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER '\0'     /*设置驱动器可访问的大写字母（如 'A'）。*/
    #define LV_FS_POSIX_PATH ""         /*设置工作目录，文件/目录路径将附加到其后。*/
    #define LV_FS_POSIX_CACHE_SIZE 0    /*>0 时在 lv_fs_read() 中缓存相应字节数。*/
#endif

/*用于 CreateFile、ReadFile 等的 API。*/
#define LV_USE_FS_WIN32 0
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER '\0'     /*设置驱动器可访问的大写字母（如 'A'）。*/
    #define LV_FS_WIN32_PATH ""         /*设置工作目录，文件/目录路径将附加到其后。*/
    #define LV_FS_WIN32_CACHE_SIZE 0    /*>0 时在 lv_fs_read() 中缓存相应字节数。*/
#endif

/*FATFS 的 API（需要单独添加），使用 f_open、f_read 等。*/
#define LV_USE_FS_FATFS 0
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER '\0'     /*设置驱动器可访问的大写字母（如 'A'）。*/
    #define LV_FS_FATFS_CACHE_SIZE 0    /*>0 时在 lv_fs_read() 中缓存相应字节数。*/
#endif

/*LittleFS 的 API（库需要单独添加），使用 lfs_file_open、lfs_file_read 等。*/
#define LV_USE_FS_LITTLEFS 0
#if LV_USE_FS_LITTLEFS
    #define LV_FS_LITTLEFS_LETTER '\0'     /*设置驱动器可访问的大写字母（如 'A'）。*/
    #define LV_FS_LITTLEFS_CACHE_SIZE 0    /*>0 时在 lv_fs_read() 中缓存相应字节数。*/
#endif

/*PNG 解码器库。*/
#define LV_USE_PNG 1

/*BMP 解码器库。*/
#define LV_USE_BMP 0

/*JPG + 分割 JPG 解码器库。
 *分割 JPG 是针对嵌入式系统优化的自定义格式。*/
#define LV_USE_SJPG 0

/*GIF 解码器库。*/
#define LV_USE_GIF 0

/*二维码库。*/
#define LV_USE_QRCODE 0

/*FreeType 库。*/
#define LV_USE_FREETYPE 0
#if LV_USE_FREETYPE
    /*FreeType 用于缓存字符的内存 [字节]（-1：不缓存）。*/
    #define LV_FREETYPE_CACHE_SIZE (16 * 1024)
    #if LV_FREETYPE_CACHE_SIZE >= 0
        /*1：位图缓存使用 sbit 缓存；0：位图缓存使用图像缓存。*/
        /*sbit 缓存：对小位图（字体大小 < 256）具有更高的内存效率。*/
        /*字体大小 >= 256 时，必须配置为图像缓存。*/
        #define LV_FREETYPE_SBIT_CACHE 0
        /*此缓存实例管理的已打开 FT_Face/FT_Size 对象的最大数量。*/
        /*（0：使用系统默认值）*/
        #define LV_FREETYPE_CACHE_FT_FACES 0
        #define LV_FREETYPE_CACHE_FT_SIZES 0
    #endif
#endif

/*Tiny TTF 库。*/
#define LV_USE_TINY_TTF 0
#if LV_USE_TINY_TTF
    /*从文件加载 TTF 数据。*/
    #define LV_TINY_TTF_FILE_SUPPORT 0
#endif

/*Rlottie 库。*/
#define LV_USE_RLOTTIE 0

/*用于图像解码和视频播放的 FFmpeg 库。
 *其支持所有主要图像格式，因此不要同时启用其他图像解码器。*/
#define LV_USE_FFMPEG 0
#if LV_USE_FFMPEG
    /*将输入信息输出到 stderr。*/
    #define LV_FFMPEG_DUMP_FORMAT 0
#endif

/*-----------
 * 其他
 *----------*/

/*1：启用为对象创建快照的 API。*/
#define LV_USE_SNAPSHOT 0

/*1：启用 Monkey 测试。*/
#define LV_USE_MONKEY 0

/*1：启用网格导航。*/
#define LV_USE_GRIDNAV 0

/*1：启用 lv_obj 片段。*/
#define LV_USE_FRAGMENT 0

/*1：支持在 label 或 span 控件中将图像用作字体。*/
#define LV_USE_IMGFONT 0

/*1：启用基于发布/订阅的消息系统。*/
#define LV_USE_MSG 0

/*1：启用拼音输入法。*/
/*需要：lv_keyboard*/
#define LV_USE_IME_PINYIN 0
#if LV_USE_IME_PINYIN
    /*1：使用默认词库。*/
    /*若不使用默认词库，设置词库后请务必使用 `lv_ime_pinyin`。*/
    #define LV_IME_PINYIN_USE_DEFAULT_DICT 1
    /*设置可显示的候选面板最大数量。*/
    /*需根据屏幕大小调整。*/
    #define LV_IME_PINYIN_CAND_TEXT_NUM 6

    /*使用九键输入（k9）。*/
    #define LV_IME_PINYIN_USE_K9_MODE      1
    #if LV_IME_PINYIN_USE_K9_MODE == 1
        #define LV_IME_PINYIN_K9_CAND_TEXT_NUM 3
    #endif // LV_IME_PINYIN_USE_K9_MODE
#endif

/*==================
* 示例
*==================*/

/*启用随库一起构建示例。*/
#define LV_BUILD_EXAMPLES 1

/*===================
 * 演示使用
 ====================*/

/*显示一些控件，可能需要增大 `LV_MEM_SIZE`。*/
#define LV_USE_DEMO_WIDGETS 0
#if LV_USE_DEMO_WIDGETS
#define LV_DEMO_WIDGETS_SLIDESHOW 0
#endif

/*演示编码器和键盘的用法。*/
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0

/*测试系统性能。*/
#define LV_USE_DEMO_BENCHMARK 0
#if LV_USE_DEMO_BENCHMARK
/*使用 16 位色深的 RGB565A8 图像替代 ARGB8565。*/
#define LV_DEMO_BENCHMARK_RGB565A8 0
#endif

/*LVGL 压力测试。*/
#define LV_USE_DEMO_STRESS 0

/*音乐播放器演示。*/
#define LV_USE_DEMO_MUSIC 0
#if LV_USE_DEMO_MUSIC
    #define LV_DEMO_MUSIC_SQUARE    0
    #define LV_DEMO_MUSIC_LANDSCAPE 0
    #define LV_DEMO_MUSIC_ROUND     0
    #define LV_DEMO_MUSIC_LARGE     0
    #define LV_DEMO_MUSIC_AUTO_PLAY 0
#endif

/*--LV_CONF_H 结束--*/

#endif /*LV_CONF_H*/

#endif /*“内容启用”结束*/
