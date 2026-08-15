#ifndef GLOG_FLASH_H
#define GLOG_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define GLOG_FLASH_ADDR_BASE  
#define GLOG_FLASH_TOTAL_SIZE  
#define GLOG_FLASH_ERASE_BLOCK_SIZE (4 * 1024) // 4KB
#define GLOG_FLASH_PAGE_SIZE (256) // 256B


/*
FLASH存储时，需要再次确定：block_seq、block_count、head_crc32
压缩时需要明确：
压缩前：raw_data_crc32、raw_data_len、rtc_time、magic
压缩后：lz4_data_crc32、lz4_data_len
*/
typedef struct __attribute__((packed)) {
    uint32_t block_seq;//flash存储时确定
    uint32_t block_count;//flash存储时确定
    uint64_t rtc_time;//压缩时获取时间戳
    uint32_t head_crc32;//flash存储时确定：只计算block_seq、block_count、rtc_time
    uint32_t raw_data_crc32;//压缩前赋值：计算原始数据的crc32
    uint32_t raw_data_len;//压缩前赋值：原始数据长度
    uint32_t lz4_data_crc32;//压缩后赋值：计算lz4压缩数据的crc32
    uint32_t lz4_data_len;//压缩后赋值：lz4压缩数据长度
    uint32_t magic;//默认为GLOG_LZ4_MAGIC
} glog_lz4_head_t;



typedef struct {
    uint32_t seq_old;      // 最旧的日志头存储的序号
    uint32_t seq_new;      // 最新的日志头存储的序号
    uint32_t seq_count;    // 日志头的总数
} glog_flash_t;

static glog_flash_t glog_flash = {
    .seq_old = 0,
    .seq_new = 0,
    .seq_count = 0,
};

// 初始化flash，扫描flash，获取flash容量、擦除块大小、页大小等信息
void glog_flash_init(void);

// 序号与地址的转换
uint32_t glog_flash_seq_to_addr(uint32_t seq);
uint32_t glog_flash_addr_to_seq(uint32_t addr);

// 写入新的日志数据到flash
void glog_flash_write(const uint8_t *data, size_t len);
// 读取flash中的最旧的日志数据
void glog_flash_read_oldest(uint8_t *data, size_t len);
// 删除指定序号的日志数据
void glog_flash_delete(uint32_t seq);




#endif /* GLOG_FLASH_H */