#ifndef W25Q64_TEST_H
#define W25Q64_TEST_H

#include "esp_err.h"

/**
 * @brief 初始化软件 SPI，执行 W25Q64 通信自检后释放资源。
 *
 * 该测试会擦除 W25Q64 最后一个 4 KiB 扇区，请勿在该区域保存业务数据。
 */
esp_err_t w25q64_communication_test(void);

#endif // W25Q64_TEST_H
