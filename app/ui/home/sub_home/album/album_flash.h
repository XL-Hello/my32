#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 由相册资源模块拥有、供页面只读使用的当前图片。 */
typedef struct {
    const uint8_t *data;
    size_t data_len;
    size_t index;
} album_flash_image_t;

/** 扫描 LittleFS 相册目录，并预加载索引 0 的图片。 */
bool album_flash_init(void);

/** 返回初始化时扫描到的 PNG 图片数量，不会重新遍历目录。 */
size_t album_flash_get_image_count(void);

/** 返回当前图片；没有可用图片或读取失败时返回 NULL。 */
const album_flash_image_t *album_flash_get_current_image(void);

/**
 * @brief 将当前图片切换到指定索引。
 *
 * 切换时会先释放原图片数据。输出指针由本模块所有，下一次切换或
 * album_flash_deinit() 后失效。
 */
bool album_flash_get_image_by_index(size_t index, const uint8_t **out_data,
                                    size_t *out_data_len);

/** 释放当前图片及模块状态。 */
void album_flash_deinit(void);
