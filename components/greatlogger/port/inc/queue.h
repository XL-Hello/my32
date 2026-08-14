#ifndef GLOG_QUEUE_H
#define GLOG_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLOG_QUEUE_OK = 0,
    GLOG_QUEUE_ERR_INVALID_ARG,
    GLOG_QUEUE_ERR_NOT_INITIALIZED,
    GLOG_QUEUE_ERR_NO_MEMORY,
    GLOG_QUEUE_ERR_FULL,
    GLOG_QUEUE_ERR_EMPTY,
    GLOG_QUEUE_ERR_BUSY,
} glog_queue_status_t;

typedef struct glog_queue_node {
    void *data;
    struct glog_queue_node *next;
} glog_queue_node_t;

typedef struct {
    glog_queue_node_t *head;
    glog_queue_node_t *tail;
    glog_queue_node_t *free_head;
    glog_queue_node_t *nodes;
    size_t length;
    size_t capacity;
    void *lock;
    void *lock_storage;
    glog_mem_pool_t *node_pool;
    glog_mem_pool_t *control_pool;
    bool initialized;
} glog_queue_t;

/* 节点一次性预分配；put/get 只移动节点，不取得 data 的释放责任。 */
glog_queue_status_t glog_queue_init(glog_queue_t *queue,glog_mem_pool_t *node_pool,glog_mem_pool_t *control_pool,size_t max_depth);
glog_queue_status_t glog_queue_put(glog_queue_t *queue, void *data);
glog_queue_status_t glog_queue_get(glog_queue_t *queue, void **data);
/* 销毁前必须由调用方 drain 所有 data，非空队列返回 BUSY。 */
glog_queue_status_t glog_queue_deinit(glog_queue_t *queue);
size_t glog_queue_length(glog_queue_t *queue);
bool glog_queue_empty(glog_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_QUEUE_H */
