#include "compress.h"

#include <limits.h>

#include "lz4.h"

#define GLOG_LZ4_ACCELERATION 1

struct glog_compressor {
    glog_mem_pool_t *pool;
    const glog_compress_backend_ops_t *ops;
    void *backend_context;
    bool initialized;
};

static bool glog_compress_io_is_valid(const void *input,
                                      size_t input_size,
                                      void *output,
                                      size_t output_capacity,
                                      size_t *output_size)
{
    if (output_size == NULL || output == NULL || output_capacity == 0U) {
        return false;
    }
    return input != NULL || input_size == 0U;
}

static size_t glog_lz4_bound(void *context, size_t input_size)
{
    int bound;

    (void)context;
    if (input_size > INT_MAX) {
        return 0U;
    }

    bound = LZ4_compressBound((int)input_size);
    return bound > 0 ? (size_t)bound : 0U;
}

static glog_compress_status_t glog_lz4_create(glog_mem_pool_t *pool, void **context)
{
    int state_size = LZ4_sizeofState();
    void *state;

    if (pool == NULL || context == NULL || state_size <= 0) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }

    state = glog_mem_pool_calloc(pool, 1U, (size_t)state_size);
    if (state == NULL) {
        return GLOG_COMPRESS_ERR_NO_MEMORY;
    }
    if (LZ4_initStream(state, (size_t)state_size) == NULL) {
        glog_mem_pool_free(pool, state);
        return GLOG_COMPRESS_ERR_INTERNAL;
    }

    *context = state;
    return GLOG_COMPRESS_OK;
}

static void glog_lz4_destroy(glog_mem_pool_t *pool, void *context)
{
    glog_mem_pool_free(pool, context);
}

static glog_compress_status_t glog_lz4_compress(
    void *context,
    const void *input,
    size_t input_size,
    void *output,
    size_t output_capacity,
    size_t *output_size
)
{
    int compressed_size;

    if (context == NULL || !glog_compress_io_is_valid(input, input_size, output,
                                   output_capacity, output_size)) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }
    if (input_size > INT_MAX || output_capacity > INT_MAX) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }

    compressed_size = LZ4_compress_fast_extState(
        context,
        (const char *)input,
        (char *)output,
        (int)input_size,
        (int)output_capacity,
        GLOG_LZ4_ACCELERATION
    );
    if (compressed_size <= 0) {
        return GLOG_COMPRESS_ERR_OUTPUT_TOO_SMALL;
    }

    *output_size = (size_t)compressed_size;
    return GLOG_COMPRESS_OK;
}

static glog_compress_status_t glog_lz4_decompress(
    void *context,
    const void *input,
    size_t input_size,
    void *output,
    size_t output_capacity,
    size_t *output_size
)
{
    int decompressed_size;

    (void)context;
    if (!glog_compress_io_is_valid(input, input_size, output,
                                   output_capacity, output_size)) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }
    if (input_size > INT_MAX || output_capacity > INT_MAX) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }

    decompressed_size = LZ4_decompress_safe(
        input,
        output,
        (int)input_size,
        (int)output_capacity
    );
    if (decompressed_size < 0) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }

    *output_size = (size_t)decompressed_size;
    return GLOG_COMPRESS_OK;
}

static const glog_compress_backend_ops_t glog_lz4_backend = {
    .name = "lz4",
    .create = glog_lz4_create,
    .destroy = glog_lz4_destroy,
    .bound = glog_lz4_bound,
    .compress = glog_lz4_compress,
    .decompress = glog_lz4_decompress,
};

const glog_compress_backend_ops_t *glog_compressor_default_backend(void)
{
    return &glog_lz4_backend;
}

glog_compressor_t *glog_compressor_create(
    glog_mem_pool_t *pool,
    const glog_compress_backend_ops_t *backend
)
{
    glog_compressor_t *compressor;
    void *context = NULL;

    if (pool == NULL || backend == NULL || backend->bound == NULL ||
        backend->compress == NULL || backend->decompress == NULL) {
        return NULL;
    }
    compressor = glog_mem_pool_calloc(pool, 1U, sizeof(*compressor));
    if (compressor == NULL) {
        return NULL;
    }
    if (backend->create != NULL &&
        backend->create(pool, &context) != GLOG_COMPRESS_OK) {
        glog_mem_pool_free(pool, compressor);
        return NULL;
    }
    compressor->pool = pool;
    compressor->ops = backend;
    compressor->backend_context = context;
    compressor->initialized = true;
    return compressor;
}

glog_compressor_t *glog_compressor_create_default(glog_mem_pool_t *pool)
{
    return glog_compressor_create(pool, glog_compressor_default_backend());
}

void glog_compressor_destroy(glog_compressor_t *compressor)
{
    if (compressor == NULL) {
        return;
    }
    if (compressor->ops->destroy != NULL) {
        compressor->ops->destroy(compressor->pool,
                                 compressor->backend_context);
    }
    glog_mem_pool_free(compressor->pool, compressor);
}

size_t glog_compressor_bound(glog_compressor_t *compressor, size_t input_size)
{
    if (compressor == NULL || !compressor->initialized) {
        return 0U;
    }
    return compressor->ops->bound(compressor->backend_context, input_size);
}

glog_compress_status_t glog_compressor_compress(
    glog_compressor_t *compressor,
    const void *input,
    size_t input_size,
    void *output,
    size_t output_capacity,
    size_t *output_size
)
{
    if (compressor == NULL || !compressor->initialized) {
        return GLOG_COMPRESS_ERR_NOT_INITIALIZED;
    }
    return compressor->ops->compress(compressor->backend_context, input,
                                     input_size, output, output_capacity,
                                     output_size);
}

glog_compress_status_t glog_compressor_decompress(
    glog_compressor_t *compressor,
    const void *input,
    size_t input_size,
    void *output,
    size_t output_capacity,
    size_t *output_size
)
{
    if (compressor == NULL || !compressor->initialized) {
        return GLOG_COMPRESS_ERR_NOT_INITIALIZED;
    }
    return compressor->ops->decompress(compressor->backend_context, input,
                                       input_size, output, output_capacity,
                                       output_size);
}
