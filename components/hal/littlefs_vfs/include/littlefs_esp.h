#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /** 注册到 ESP-IDF VFS 的路径前缀，例如 /littlefs。 */
    const char *base_path;
    /** LittleFS 数据分区标签，例如 littlefs。 */
    const char *partition_label;
    /** 仅在检测到未格式化的 LittleFS 元数据时格式化分区。 */
    bool format_if_mount_failed;
} littlefs_esp_config_t;

/**
 * 挂载 LittleFS 并注册 VFS。成功后可使用 fopen、fread、fwrite、fsync 和 fclose
 * 访问 base_path 下的文件，也可使用 opendir、readdir、closedir、mkdir、rmdir 和
 * rename 操作目录。
 */
esp_err_t littlefs_esp_mount(const littlefs_esp_config_t *config);

/** 卸载 LittleFS 并注销 VFS。 */
esp_err_t littlefs_esp_unmount(void);

#ifdef __cplusplus
}
#endif
