#ifndef GLOG_H
#define GLOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t glog_seq_t;

typedef enum {
    GLOG_OK = 0,
    GLOG_ERR
} glog_status_t;

/* 使用组件内部默认配置初始化日志流水线,初始化后即可开始接收日志。 */
glog_status_t glog_init(void);

/* 停止接收新日志，排空流水线并释放内部资源。 */
glog_status_t glog_deinit(void);

/* 非阻塞、整段接收或整段拒绝。 
* 将data_len的数据放入循环缓冲区，返回GLOG_OK表示成功接收，返回GLOG_ERR表示缓冲区已满，无法接收。
* 单buffer，数据满时直接丢弃新数据；双buffer，数据满时切换buffer，旧buffer异步压缩并持久化，新buffer继续接收数据。
* 该函数是线程安全的，可在中断上下文中调用。
*/
glog_status_t glog_put(const void *data, size_t data_len);
/*
 * 复制最旧的完整日志包，但不删除它。
 */
glog_seq_t glog_get_oldest(void *buf,size_t buf_size, size_t *package_size);

/* 删除指定序号的日志包。 */
glog_status_t glog_delete_oldest(glog_seq_t seq);


/* 打印内存池状态。 */
glog_status_t glog_print_pool_stats(void);


#ifdef __cplusplus
}
#endif

#endif /* GLOG_H */
