#pragma once

#include <stddef.h>
#include <stdint.h>

/** 创建完整相册浏览页，图片数据必须来自 album_flash。 */
void album_create(const uint8_t *data, size_t data_len, size_t image_count);

/** 清空图片源并重置页面引用，不会释放 album_flash 管理的数据。 */
void album_destroy(void);
