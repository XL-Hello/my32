#include "glog_flash.h"




/*
遍历所有block，读取前sizeof(glog_lz4_head_t)字节，判断magic是否为GLOG_LZ4_MAGIC
如果不是，跳过
如果是，重新计算block_seq、block_count、rtc_time的crc32，判断是否与head_crc32一致。如果不一致，则跳过。
如果满足条件，说明该block是有效的日志数据，则需要更新seq_old、seq_new、seq_count，逻辑为：
    若seq_count为0，seq_old和seq_new直接赋值当前block的序号；
    若seq_count不为0，则需要根据rtc_time判断更新seq_old或者seq_new；
*/
void glog_flash_scan(void)
{

}


void glog_flash_init(void)
{
    glog_flash_scan();
}


/*
先判断


*/
void glog_flash_write(const uint8_t *data, size_t len)
{




}