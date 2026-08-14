#ifndef GLOG_COMPRESS_H
#define GLOG_COMPRESS_H

#include <stddef.h>
#include "mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glog_compressor glog_compressor_t;

typedef enum {
    GLOG_COMPRESS_OK = 0,
    GLOG_COMPRESS_ERR_INVALID_ARG,
    GLOG_COMPRESS_ERR_NO_MEMORY,
    GLOG_COMPRESS_ERR_INVALID_DATA,
    GLOG_COMPRESS_ERR_OUTPUT_TOO_SMALL,
    GLOG_COMPRESS_ERR_UNSUPPORTED,
    GLOG_COMPRESS_ERR_NOT_INITIALIZED,
    GLOG_COMPRESS_ERR_INTERNAL,
} glog_compress_status_t;

typedef struct glog_compress_backend_ops {
    const char *name;

    glog_compress_status_t (*create)(glog_mem_pool_t *pool, void **context);

    void (*destroy)(glog_mem_pool_t *pool, void *context);

    size_t (*bound)(void *context, size_t input_size);

    glog_compress_status_t (*compress)(void *context, const void *input, size_t input_size, void *output, size_t output_capacity, size_t *output_size);

    glog_compress_status_t (*decompress)(void *context, const void *input, size_t input_size, void *output, size_t output_capacity, size_t *output_size);
} glog_compress_backend_ops_t;

/* 使用指定后端创建压缩器。 */
glog_compressor_t *glog_compressor_create(glog_mem_pool_t *pool, const glog_compress_backend_ops_t *backend);

/* 使用默认后端创建压缩器。 */
glog_compressor_t *glog_compressor_create_default(glog_mem_pool_t *pool);

/* 销毁压缩器并释放其资源。 */
void glog_compressor_destroy(glog_compressor_t *compressor);

/* 获取指定输入长度的最大压缩输出长度。 */
size_t glog_compressor_bound(glog_compressor_t *compressor,size_t input_size);

/* 将输入数据压缩到调用方提供的输出缓冲区。 */
glog_compress_status_t glog_compressor_compress(glog_compressor_t *compressor, const void *input, size_t input_size, void *output, size_t output_capacity, size_t *output_size);

/* 将输入数据解压到调用方提供的输出缓冲区。 */
glog_compress_status_t glog_compressor_decompress(glog_compressor_t *compressor, const void *input, size_t input_size, void *output, size_t output_capacity, size_t *output_size);

/* 获取默认压缩后端。 */
const glog_compress_backend_ops_t *glog_compressor_default_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_COMPRESS_H */
