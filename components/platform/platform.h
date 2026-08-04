#pragma once

#include <stddef.h>
#include <stdlib.h>

#include "esp_heap_caps.h"

/**
 * @brief 从 PSRAM 分配可按字节访问的动态内存。
 *
 * 使用标准 free() 释放。
 */
static inline void *ps_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/**
 * @brief 从 PSRAM 分配并清零数组。
 *
 * 使用标准 free() 释放。
 */
static inline void *ps_calloc(size_t count, size_t size)
{
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

/** @brief 将动态内存调整到 PSRAM。 */
static inline void *ps_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
