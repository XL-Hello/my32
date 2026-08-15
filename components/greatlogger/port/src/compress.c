#include "compress.h"

#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "glog_crc32.h"
#include "lz4.h"
#include "platform.h"
#include "glog_flash.h"

#define GLOG_LZ4_ACCELERATION 1
#define GLOG_LZ4_MAGIC 0x55AA66BBU

/*
 * This value must match the allocation unit selected by the persistent log
 * storage layer.  It is deliberately a build-time override because the
 * compressor API does not own that layer's configuration.
 */
#ifndef GLOG_LZ4_STORAGE_BLOCK_SIZE
#define GLOG_LZ4_STORAGE_BLOCK_SIZE 4096U
#endif

/* 默认关闭，调试封包头时通过编译选项设为 1。 */
#ifndef GLOG_LZ4_DEBUG_DUMP_HEAD
#define GLOG_LZ4_DEBUG_DUMP_HEAD 1
#endif

#if GLOG_LZ4_DEBUG_DUMP_HEAD
#define LOG_TAG "glog_lz4"
#include "platform_log.h"
#endif

_Static_assert(sizeof(glog_lz4_head_t) == 40U,
               "glog LZ4 packet header must be 40 bytes");

typedef struct {
    uint64_t previous_rtc_time;
    LZ4_stream_t stream;
} glog_lz4_context_t;

struct glog_compressor {
    glog_mem_pool_t *pool;
    const glog_compress_backend_ops_t *ops;
    void *backend_context;
    bool initialized;
};

static uint32_t glog_lz4_head_crc32(const glog_lz4_head_t *head)
{
    glog_crc32_context_t crc;

    glog_crc32_init(&crc);
    glog_crc32_update(&crc, &head->block_seq, sizeof(head->block_seq));
    glog_crc32_update(&crc, &head->block_count, sizeof(head->block_count));
    glog_crc32_update(&crc, &head->rtc_time, sizeof(head->rtc_time));
    return glog_crc32_final(&crc);
}

#if GLOG_LZ4_DEBUG_DUMP_HEAD
static void glog_lz4_dump_head(const glog_lz4_head_t *head)
{
    log_info("LZ4_HEAD: block_seq=%" PRIu32 ", block_count=%" PRIu32
             ", rtc_time=%" PRIu64 ", head_crc32=%" PRIu32
             ", raw_data_crc32=%" PRIu32
             ", lz4_data_crc32=%" PRIu32 ", magic=0x%08" PRIx32,
             head->block_seq, head->block_count, head->rtc_time,
             head->head_crc32, head->raw_data_crc32, head->lz4_data_crc32,
             head->magic);
}
#endif

static bool glog_lz4_packet_size(size_t lz4_data_len, size_t *packet_size)
{
    if (packet_size == NULL || lz4_data_len > SIZE_MAX - sizeof(glog_lz4_head_t)) {
        return false;
    }

    *packet_size = sizeof(glog_lz4_head_t) + lz4_data_len;
    return true;
}

static bool glog_lz4_block_count(size_t packet_size, uint32_t *block_count)
{
    size_t count;

    if (block_count == NULL || GLOG_LZ4_STORAGE_BLOCK_SIZE == 0U ||
        packet_size > SIZE_MAX - (GLOG_LZ4_STORAGE_BLOCK_SIZE - 1U)) {
        return false;
    }

    count = (packet_size + GLOG_LZ4_STORAGE_BLOCK_SIZE - 1U) /
            GLOG_LZ4_STORAGE_BLOCK_SIZE;
    if (count == 0U || count > UINT32_MAX) {
        return false;
    }

    *block_count = (uint32_t)count;
    return true;
}

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
    if (bound <= 0 || (size_t)bound > SIZE_MAX - sizeof(glog_lz4_head_t)) {
        return 0U;
    }
    return sizeof(glog_lz4_head_t) + (size_t)bound;
}

static glog_compress_status_t glog_lz4_create(glog_mem_pool_t *pool, void **context)
{
    glog_lz4_context_t *lz4_context;

    if (pool == NULL || context == NULL) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }

    lz4_context = glog_mem_pool_calloc(pool, 1U, sizeof(*lz4_context));
    if (lz4_context == NULL) {
        return GLOG_COMPRESS_ERR_NO_MEMORY;
    }
    if (LZ4_initStream(&lz4_context->stream, sizeof(lz4_context->stream)) == NULL) {
        glog_mem_pool_free(pool, lz4_context);
        return GLOG_COMPRESS_ERR_INTERNAL;
    }

    *context = lz4_context;
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
    glog_lz4_context_t *lz4_context = context;
    glog_lz4_head_t head;
    const uint8_t empty_input = 0U;
    const void *lz4_input = input != NULL ? input : &empty_input;
    uint8_t *lz4_output;
    int compressed_size;
    size_t packet_size;
    uint32_t raw_data_crc32;
    uint64_t rtc_time;

    if (lz4_context == NULL || !glog_compress_io_is_valid(input, input_size, output,
                                   output_capacity, output_size)) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }
    if (input_size > INT_MAX || input_size > UINT32_MAX ||
        output_capacity < sizeof(head)) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }
    raw_data_crc32 = glog_crc32_compute(input, input_size);

    lz4_output = (uint8_t *)output + sizeof(head);
    if (output_capacity - sizeof(head) > INT_MAX) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }

    compressed_size = LZ4_compress_fast_extState(
        &lz4_context->stream,
        (const char *)lz4_input,
        (char *)lz4_output,
        (int)input_size,
        (int)(output_capacity - sizeof(head)),
        GLOG_LZ4_ACCELERATION
    );
    if (compressed_size <= 0) {
        return GLOG_COMPRESS_ERR_OUTPUT_TOO_SMALL;
    }
    if (!glog_lz4_packet_size((size_t)compressed_size, &packet_size)) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }
    rtc_time = glog_rtc_time_ms();
    if (rtc_time <= lz4_context->previous_rtc_time) {
        if (lz4_context->previous_rtc_time == UINT64_MAX) {
            return GLOG_COMPRESS_ERR_UNSUPPORTED;
        }
        rtc_time = lz4_context->previous_rtc_time + 1U;
    }

    memset(&head, 0, sizeof(head));
    head.rtc_time = rtc_time;
    head.raw_data_crc32 = raw_data_crc32;
    head.raw_data_len = (uint32_t)input_size;
    head.lz4_data_crc32 = glog_crc32_compute(lz4_output, (size_t)compressed_size);
    head.lz4_data_len = (uint32_t)compressed_size;
    head.magic = GLOG_LZ4_MAGIC;

    memcpy(output, &head, sizeof(head));
    lz4_context->previous_rtc_time = head.rtc_time;
    *output_size = packet_size;
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
    glog_lz4_head_t head;
    size_t packet_size;
    uint32_t block_count;
    bool flash_head_finalized;
    int decompressed_size;

    (void)context;
    if (!glog_compress_io_is_valid(input, input_size, output,
                                   output_capacity, output_size)) {
        return GLOG_COMPRESS_ERR_INVALID_ARG;
    }
    if (input_size < sizeof(head)) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }

    memcpy(&head, input, sizeof(head));
#if GLOG_LZ4_DEBUG_DUMP_HEAD
    glog_lz4_dump_head(&head);
#endif
    if (head.magic != GLOG_LZ4_MAGIC) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }
    if (!glog_lz4_packet_size(head.lz4_data_len, &packet_size) ||
        packet_size != input_size) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }
    flash_head_finalized = head.block_seq != 0U || head.block_count != 0U ||
                           head.head_crc32 != 0U;
    if (flash_head_finalized &&
        (head.head_crc32 != glog_lz4_head_crc32(&head) ||
         !glog_lz4_block_count(packet_size, &block_count) ||
         head.block_count != block_count)) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }
    if (head.raw_data_len > output_capacity ||
        head.lz4_data_len > INT_MAX || head.raw_data_len > INT_MAX) {
        return GLOG_COMPRESS_ERR_UNSUPPORTED;
    }
    if (glog_crc32_compute((const uint8_t *)input + sizeof(head),
                           head.lz4_data_len) != head.lz4_data_crc32) {
        return GLOG_COMPRESS_ERR_INVALID_DATA;
    }

    decompressed_size = LZ4_decompress_safe(
        (const char *)input + sizeof(head),
        (char *)output,
        (int)head.lz4_data_len,
        (int)head.raw_data_len
    );
    if (decompressed_size < 0 || (uint32_t)decompressed_size != head.raw_data_len ||
        glog_crc32_compute(output, (size_t)decompressed_size) != head.raw_data_crc32) {
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
