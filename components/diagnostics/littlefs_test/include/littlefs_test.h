#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 创建、写入、读取并校验一个 LittleFS 测试文件。 */
esp_err_t littlefs_esp_test(void);

/*
示例输出20260731：
I (443) littlefs: (littlefs_esp_test:66): read test file: HELLO LittleFS
I (8533) littlefs: (littlefs_log_speed:24): write 512 KiB: 8088157 us, 63 KiB/s
I (8903) littlefs: (littlefs_log_speed:24): read 512 KiB: 365953 us, 1399 KiB/s
*/


/** 测量 512 KiB 文件的 LittleFS 顺序写入与完整读取速率。 */
esp_err_t littlefs_esp_speed_test(void);

#ifdef __cplusplus
}
#endif
