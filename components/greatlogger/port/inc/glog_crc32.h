
#ifndef GLOG_CRC32_H
#define GLOG_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t value;
} glog_crc32_context_t;

void glog_crc32_init(glog_crc32_context_t *context);
void glog_crc32_update(glog_crc32_context_t *context, const void *data, size_t length);
uint32_t glog_crc32_final(const glog_crc32_context_t *context);
uint32_t glog_crc32_compute(const void *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_CRC32_H */
