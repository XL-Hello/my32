#include "platform.h"
#include "platform_log.h"
#include "mempool.h"
#include "ringbuffer.h"
#include "queue.h"
#include "compress.h"

#include <string.h>
#include <stdio.h>

#define GLOG_DEFAULT_BULK_POOL_SIZE (1024 * 256)
#define GLOG_DEFAULT_COLLECTOR_SIZE (1024 * 32)
#define GLOG_COMPRESS_QUEUE_DEPTH   (4U)

void glog_compress_task(void *arg);
void glog_persist_task(void *arg);
glog_status_t glog_test_start(void);

volatile bool glog_initialized = false;//日志模块是否完成初始化

glog_mem_pool_t *glog_bulk_pool = NULL;//内存池，用于日志数据的动态分配
static glog_ringbuf_t glog_ringbuf = {0};//环形缓冲区，用于日志数据的收集和存储
static glog_queue_t glog_compress_queue = {0};
static glog_queue_t glog_persist_queue = {0};
static glog_compressor_t *glog_compressor = NULL;
static void *glog_compress_task_handle = NULL;
static void *glog_persist_task_handle = NULL;

typedef struct{
    size_t data_len;
    size_t original_data_len;
    uint8_t data[];
} data_node_t;

glog_status_t glog_init(void)
{
    // 初始化内存池，使用components/greatlogger/port/inc/mempool.h的接口，分配GLOG_DEFAULT_BULK_POOL_SIZE大小的内存池
    glog_bulk_pool = glog_mem_pool_create(GLOG_DEFAULT_BULK_POOL_SIZE);
    // 分配日志缓冲区，使用components/greatlogger/port/inc/ringbuffer.h接口，分配一个GLOG_DEFAULT_COLLECTOR_SIZE大小的单缓冲区环形缓冲区
    if(glog_rb_init(&glog_ringbuf, glog_mem_pool_malloc(glog_bulk_pool, GLOG_DEFAULT_COLLECTOR_SIZE), GLOG_DEFAULT_COLLECTOR_SIZE))
    {
        log_info("glog_ringbuf init success");
    }
    //初始化一个链表队列,用于循环buffer满时入队并通知到压缩任务。
    if (glog_queue_init(&glog_compress_queue, glog_bulk_pool,
                        glog_bulk_pool, GLOG_COMPRESS_QUEUE_DEPTH) !=
        GLOG_QUEUE_OK) {
        return GLOG_ERR;
    }
    if (glog_queue_init(&glog_persist_queue, glog_bulk_pool,
                        glog_bulk_pool, GLOG_COMPRESS_QUEUE_DEPTH) !=
        GLOG_QUEUE_OK) {
        return GLOG_ERR;
    }

    //创建压缩器
    glog_compressor = glog_compressor_create_default(glog_bulk_pool);

    // 创建两个任务，一个是压缩任务，一个是持久化任务，使用components/greatlogger/port/inc/task.h接口，分别设置任务栈大小和优先级为GLOG_DEFAULT_COMPRESSION_STACK_SIZE、GLOG_DEFAULT_COMPRESSION_PRIORITY和GLOG_DEFAULT_PERSIST_STACK_SIZE、GLOG_DEFAULT_PERSIST_PRIORITY
    if (glog_task_create("glog_compress", glog_compress_task, NULL,
                         &glog_compress_task_handle) != GLOG_OK ||
        glog_task_create("glog_persist", glog_persist_task, NULL,
                         &glog_persist_task_handle) != GLOG_OK) {
        return GLOG_ERR;
    }

    if (glog_test_start() != GLOG_OK) {
        return GLOG_ERR;
    }

    return GLOG_OK;
}

glog_status_t glog_put(const void *data, size_t data_len)
{
    if(data == NULL || data_len == 0)
    {
        return GLOG_ERR;
    }

    size_t rb_avail = glog_rb_avail(&glog_ringbuf);
    if(rb_avail < data_len)
    {
        size_t rb_len = glog_rb_length(&glog_ringbuf);//数据长度
        //从内存池申请内存
        data_node_t *buf = (data_node_t*)glog_mem_pool_malloc(glog_bulk_pool, rb_len + sizeof(data_node_t));
        if(buf == NULL)
        {
            log_error("glog_mem_pool_malloc failed");
            return GLOG_ERR;
        }
        //从环形缓冲区中取出数据
        glog_rb_get(&glog_ringbuf, buf->data, rb_len);
        buf->data_len = rb_len;
        buf->original_data_len = rb_len;
        //将buf的数据加入压缩链表队列，若失败，释放内存，
        if (glog_queue_put(&glog_compress_queue, buf) != GLOG_QUEUE_OK) {
            // 失败->释放内存
            log_error("glog_queue_put failed, free buf");
            glog_mem_pool_free(glog_bulk_pool, buf);
        } else{
            // 成功->通知压缩任务有数据需要处理
            log_error("glog_ringbuf is full, data_len=%d", rb_len);
            glog_task_notify(glog_compress_task_handle);
        }

        // 重置环形缓冲区
        glog_rb_clean(&glog_ringbuf);
        rb_avail = glog_rb_avail(&glog_ringbuf);
        if(rb_avail >= data_len)
        {
            glog_rb_put(&glog_ringbuf, data, data_len);
            return GLOG_OK;
        }
    } else {
        glog_rb_put(&glog_ringbuf, data, data_len);
        return GLOG_OK;
    }
    return GLOG_OK;
}



void glog_compress_task(void *arg)
{
    // 压缩任务的实现，循环从环形缓冲区中获取日志数据，进行压缩处理，并将压缩后的数据放入持久化队列中
    while(1)
    {
        glog_task_notify_wait(UINT32_MAX);
        //取出链表队列中的数据
        data_node_t *buf = NULL;
        if (glog_queue_get(&glog_compress_queue, (void **)&buf) == GLOG_QUEUE_OK) {
            size_t dump_len = buf->data_len < 48U ? buf->data_len : 48U;
            ESP_LOGI("glog_compress", "原始数据，长度=%u", (unsigned int)buf->data_len);
            ESP_LOG_BUFFER_HEXDUMP("glog_compress", buf->data, dump_len, ESP_LOG_INFO);
            size_t compressed_bound = glog_compressor_bound(glog_compressor, buf->data_len);
            if (compressed_bound == 0U || compressed_bound > SIZE_MAX - sizeof(data_node_t)) {
                log_error("glog_compressor_bound failed");
                glog_mem_pool_free(glog_bulk_pool, buf);
                continue;
            }
            log_error("glog_compressor_bound: %d", compressed_bound);
            data_node_t *persist_buf = glog_mem_pool_malloc(glog_bulk_pool, sizeof(data_node_t) + compressed_bound);
            if (persist_buf == NULL) {
                log_error("glog_mem_pool_malloc failed");
                glog_mem_pool_free(glog_bulk_pool, buf);
                continue;
            }

            if (glog_compressor_compress(glog_compressor,
                                         buf->data,
                                         buf->data_len,
                                         persist_buf->data,
                                         compressed_bound,
                                         &persist_buf->data_len) != GLOG_COMPRESS_OK) {
                log_error("glog_compressor_compress failed");
                glog_mem_pool_free(glog_bulk_pool, persist_buf);
                glog_mem_pool_free(glog_bulk_pool, buf);
                continue;
            }
            persist_buf->original_data_len = buf->data_len;

            //将压缩后的数据加入持久化队列，若失败，释放内存
            if (glog_queue_put(&glog_persist_queue, persist_buf) != GLOG_QUEUE_OK) {
                log_error("glog_queue_put failed, free persist_buf");
                glog_mem_pool_free(glog_bulk_pool, persist_buf);
            } else {
                // 成功->通知持久化任务有数据需要处理
                log_info("(%d)---compress-->(%d)\n", buf->data_len, persist_buf->data_len);
                glog_task_notify(glog_persist_task_handle);
            }
            //释放buf内存
            glog_mem_pool_free(glog_bulk_pool, buf);
        } else {
            log_info("glog_compress_task: no data to compress\n");
        }
    }
    
}

void glog_persist_task(void *arg)
{
    // 持久化任务的实现，循环从持久化队列中获取数据，进行持久化处理
    while(1)
    {
        data_node_t *buf = NULL;
        glog_task_notify_wait(UINT32_MAX);
        if (glog_queue_get(&glog_persist_queue, (void **)&buf) ==
            GLOG_QUEUE_OK) {
            uint8_t *decompressed_data = glog_mem_pool_malloc(glog_bulk_pool, buf->original_data_len);
            if (decompressed_data == NULL) {
                log_error("glog_mem_pool_malloc failed");
            } else {
                size_t decompressed_len = 0U;
                if (glog_compressor_decompress(glog_compressor,
                                               buf->data,
                                               buf->data_len,
                                               decompressed_data,
                                               buf->original_data_len,
                                               &decompressed_len) != GLOG_COMPRESS_OK) {
                    log_error("glog_compressor_decompress failed");
                } else {
                    size_t dump_len = decompressed_len < 48U ? decompressed_len : 48U;
                    ESP_LOGI("glog_persist", "解压数据，长度=%u", (unsigned int)decompressed_len);
                    ESP_LOG_BUFFER_HEXDUMP("glog_persist", decompressed_data, dump_len, ESP_LOG_INFO);
                }
                glog_mem_pool_free(glog_bulk_pool, decompressed_data);
            }
            // 模拟持久化任务的处理时间
            glog_delay_ms(500);
            log_info("---persist-->(%d)\n", buf->data_len);
            glog_mem_pool_free(glog_bulk_pool, buf);
        }
    }
}

glog_status_t glog_print_pool_stats(void)
{
    return glog_mem_pool_print_stats(glog_bulk_pool, "glog_bulk_pool");
}