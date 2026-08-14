#include "mempool.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "lwmem/lwmem.h"
#include "platform.h"
#include "platform_log.h"
#include "../../../platform/platform.h"

#define GLOG_MEM_SIZE_TEXT_LENGTH 16U
#define GLOG_MEM_KIB_THRESHOLD (10U * 1024U)
#define GLOG_MEM_FREE_HEADER_FORMAT "%10s %10s %10s %10s %10s %6s %6s %s"
#define GLOG_MEM_FREE_ROW_FORMAT "%10s %10s %10s %10s %10s %6u %6u %s"

typedef struct {
    void *memory;
    size_t size;
    lwmem_t allocator;
} glog_lwmem_context_t;

struct glog_mem_pool {
    glog_lwmem_context_t context;
    void *lifecycle_lock;
    uint32_t allocation_failures;
    bool initialized;
};

static void glog_mem_format_size(size_t bytes, char *text, size_t text_length)
{
    if (bytes > GLOG_MEM_KIB_THRESHOLD) {
        size_t kib = bytes / 1024U;
        size_t decimal = ((bytes % 1024U) * 10U) / 1024U;

        (void)snprintf(text, text_length, "%u.%u KiB", (unsigned int)kib, (unsigned int)decimal);
        return;
    }

    (void)snprintf(text, text_length, "%u B", (unsigned int)bytes);
}

static void glog_mem_get_free_block_stats(const lwmem_t *allocator, size_t *largest_free_block, size_t *free_blocks)
{
    const lwmem_block_t *block = allocator->start_block.next;

    *largest_free_block = 0U;
    *free_blocks = 0U;
    while (block != NULL) {
        if (block->size > *largest_free_block) {
            *largest_free_block = block->size;
        }
        (*free_blocks)++;
        block = block->next;
    }
}

static bool glog_mem_is_valid_pool_size(size_t size)
{
    return size >= GLOG_MEM_POOL_MIN_REGION_SIZE &&
           (size & (size - 1U)) == 0U;
}

glog_mem_pool_t *glog_mem_pool_create(size_t region_size)
{
    glog_mem_pool_t *pool;
    void *region;
    lwmem_region_t regions[2];

    if (!glog_mem_is_valid_pool_size(region_size)) {
        return NULL;
    }

    pool = glog_ps_malloc(sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }
    memset(pool, 0, sizeof(*pool));

    region = glog_ps_malloc(region_size);
    if (region == NULL) {
        glog_free(pool);
        return NULL;
    }

    pool->lifecycle_lock = glog_mutex_create();
    if (pool->lifecycle_lock == NULL) {
        glog_free(region);
        glog_free(pool);
        return NULL;
    }

    pool->context.memory = region;
    pool->context.size = region_size;
    regions[0].start_addr = region;
    regions[0].size = region_size;
    regions[1].start_addr = NULL;
    regions[1].size = 0U;
    if (lwmem_assignmem_ex(&pool->context.allocator, regions) == 0U) {
        glog_mutex_destroy(pool->lifecycle_lock);
        glog_free(region);
        glog_free(pool);
        return NULL;
    }

    pool->initialized = true;
    return pool;
}

glog_mem_status_t glog_mem_pool_destroy(glog_mem_pool_t *pool)
{
    lwmem_stats_t stats;
    void *region;

    if (pool == NULL) {
        return GLOG_MEM_ERR_INVALID_ARG;
    }
    if (!pool->initialized) {
        return GLOG_MEM_ERR_NOT_INITIALIZED;
    }
    if (glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return GLOG_MEM_ERR_BUSY;
    }

    lwmem_get_stats_ex(&pool->context.allocator, &stats);
    if (stats.nr_alloc < stats.nr_free || stats.nr_alloc != stats.nr_free) {
        glog_mutex_unlock(pool->lifecycle_lock);
        return GLOG_MEM_ERR_BUSY;
    }

    pool->initialized = false;
    glog_mutex_unlock(pool->lifecycle_lock);
    glog_mutex_destroy(pool->lifecycle_lock);
    region = pool->context.memory;
    glog_free(region);
    glog_free(pool);
    return GLOG_MEM_OK;
}

void *glog_mem_pool_malloc(glog_mem_pool_t *pool, size_t size)
{
    void *ptr;

    if (pool == NULL || !pool->initialized || size == 0U ||
        glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return NULL;
    }
    ptr = lwmem_malloc_ex(&pool->context.allocator, NULL, size);
    if (ptr == NULL) {
        pool->allocation_failures++;
    }
    glog_mutex_unlock(pool->lifecycle_lock);
    return ptr;
}

void *glog_mem_pool_calloc(glog_mem_pool_t *pool, size_t count, size_t size)
{
    void *ptr;

    if (pool == NULL || !pool->initialized || count == 0U || size == 0U ||
        count > SIZE_MAX / size ||
        glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return NULL;
    }
    ptr = lwmem_calloc_ex(&pool->context.allocator, NULL, count, size);
    if (ptr == NULL) {
        pool->allocation_failures++;
    }
    glog_mutex_unlock(pool->lifecycle_lock);
    return ptr;
}

void *glog_mem_pool_realloc(glog_mem_pool_t *pool, void *ptr, size_t size)
{
    void *new_ptr;

    if (pool == NULL || !pool->initialized) {
        return NULL;
    }
    if (size == 0U) {
        glog_mem_pool_free(pool, ptr);
        return NULL;
    }
    if (glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return NULL;
    }
    new_ptr = lwmem_realloc_ex(&pool->context.allocator, NULL, ptr, size);
    if (new_ptr == NULL) {
        pool->allocation_failures++;
    }
    glog_mutex_unlock(pool->lifecycle_lock);
    return new_ptr;
}

void glog_mem_pool_free(glog_mem_pool_t *pool, void *ptr)
{
    if (pool == NULL || !pool->initialized || ptr == NULL ||
        glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return;
    }
    lwmem_free_ex(&pool->context.allocator, ptr);
    glog_mutex_unlock(pool->lifecycle_lock);
}

void glog_mem_pool_free_s(glog_mem_pool_t *pool, void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        glog_mem_pool_free(pool, *ptr);
        *ptr = NULL;
    }
}

bool glog_mem_pool_realloc_s(glog_mem_pool_t *pool, void **ptr, size_t size)
{
    void *new_ptr;

    if (pool == NULL || ptr == NULL || !pool->initialized) {
        return false;
    }
    if (size == 0U) {
        glog_mem_pool_free_s(pool, ptr);
        return true;
    }

    new_ptr = glog_mem_pool_realloc(pool, *ptr, size);
    if (new_ptr == NULL) {
        return false;
    }
    *ptr = new_ptr;
    return true;
}

glog_mem_status_t glog_mem_pool_get_stats(glog_mem_pool_t *pool,
                                          glog_mem_stats_t *stats)
{
    lwmem_stats_t lwstats;

    if (pool == NULL || stats == NULL) {
        return GLOG_MEM_ERR_INVALID_ARG;
    }
    if (!pool->initialized) {
        return GLOG_MEM_ERR_NOT_INITIALIZED;
    }
    if (glog_mutex_lock(pool->lifecycle_lock) != GLOG_OK) {
        return GLOG_MEM_ERR_BUSY;
    }

    lwmem_get_stats_ex(&pool->context.allocator, &lwstats);
    stats->total_bytes = lwstats.mem_size_bytes;
    stats->available_bytes = lwstats.mem_available_bytes;
    stats->minimum_available_bytes = lwstats.minimum_ever_mem_available_bytes;
    glog_mem_get_free_block_stats(&pool->context.allocator, &stats->largest_free_block, &stats->free_blocks);
    stats->current_allocations = lwstats.nr_alloc >= lwstats.nr_free
                                     ? (size_t)(lwstats.nr_alloc - lwstats.nr_free)
                                     : 0U;
    stats->allocation_count = lwstats.nr_alloc;
    stats->free_count = lwstats.nr_free;
    stats->allocation_failures = pool->allocation_failures;
    glog_mutex_unlock(pool->lifecycle_lock);
    return GLOG_MEM_OK;
}

glog_mem_status_t glog_mem_pool_print_stats(glog_mem_pool_t *pool, const char *name)
{
    glog_mem_stats_t stats;
    char total[GLOG_MEM_SIZE_TEXT_LENGTH];
    char used[GLOG_MEM_SIZE_TEXT_LENGTH];
    char free[GLOG_MEM_SIZE_TEXT_LENGTH];
    char maxused[GLOG_MEM_SIZE_TEXT_LENGTH];
    char maxfree[GLOG_MEM_SIZE_TEXT_LENGTH];
    glog_mem_status_t status;

    status = glog_mem_pool_get_stats(pool, &stats);
    if (status != GLOG_MEM_OK) {
        return status;
    }

    glog_mem_format_size(stats.total_bytes, total, sizeof(total));
    glog_mem_format_size(stats.total_bytes - stats.available_bytes, used, sizeof(used));
    glog_mem_format_size(stats.available_bytes, free, sizeof(free));
    glog_mem_format_size(stats.total_bytes - stats.minimum_available_bytes, maxused, sizeof(maxused));
    glog_mem_format_size(stats.largest_free_block, maxfree, sizeof(maxfree));
    log_printf(GLOG_MEM_FREE_HEADER_FORMAT, "total", "used", "free", "maxused", "maxfree", "nused", "nfree", "name");
    log_printf(GLOG_MEM_FREE_ROW_FORMAT,
               total,
               used,
               free,
               maxused,
               maxfree,
               (unsigned int)stats.current_allocations,
               (unsigned int)stats.free_blocks,
               name != NULL ? name : "glog_pool");
    return GLOG_MEM_OK;
}
