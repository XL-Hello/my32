#ifndef GLOG_MEMPOOL_H
#define GLOG_MEMPOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLOG_MEM_POOL_MIN_REGION_SIZE (16U * 1024U)

typedef enum {
    GLOG_MEM_OK = 0,
    GLOG_MEM_ERR_INVALID_ARG,
    GLOG_MEM_ERR_INVALID_SIZE,
    GLOG_MEM_ERR_NO_MEMORY,
    GLOG_MEM_ERR_NOT_INITIALIZED,
    GLOG_MEM_ERR_BUSY,
} glog_mem_status_t;

typedef struct {
    size_t total_bytes;
    size_t available_bytes;
    size_t minimum_available_bytes;
    /* 后端不支持最大连续空闲块查询时为 0。 */
    size_t largest_free_block;
    size_t current_allocations;
    size_t free_blocks;
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t allocation_failures;
} glog_mem_stats_t;

typedef struct glog_mem_pool glog_mem_pool_t;

/*
 * 初始化时从 PSRAM 一次性申请 pool control 与 backing region。
 * region_size 必须至少为 16 KiB 且为 2 的幂；destroy 会释放这两块内存。
 */
glog_mem_pool_t *glog_mem_pool_create(size_t region_size);
glog_mem_status_t glog_mem_pool_destroy(glog_mem_pool_t *pool);

void *glog_mem_pool_malloc(glog_mem_pool_t *pool, size_t size);
void *glog_mem_pool_calloc(glog_mem_pool_t *pool, size_t count, size_t size);
void *glog_mem_pool_realloc(glog_mem_pool_t *pool, void *ptr, size_t size);
void glog_mem_pool_free(glog_mem_pool_t *pool, void *ptr);

void glog_mem_pool_free_s(glog_mem_pool_t *pool, void **ptr);
bool glog_mem_pool_realloc_s(glog_mem_pool_t *pool, void **ptr, size_t size);

glog_mem_status_t glog_mem_pool_get_stats(glog_mem_pool_t *pool, glog_mem_stats_t *stats);
glog_mem_status_t glog_mem_pool_print_stats(glog_mem_pool_t *pool, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_MEMPOOL_H */
