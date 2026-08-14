#ifndef GLOG_RINGBUFFER_H
#define GLOG_RINGBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 单生产者、单消费者的字节环形缓冲，不在内部加锁。
 * 多任务或 task/ISR 共用时，调用方必须用外部临界区串行化所有操作。
 */
typedef struct {
    uint16_t head;
    uint16_t tail;
    uint8_t *data;
    uint16_t mask;
    bool initialized;
} glog_ringbuf_t;

bool glog_rb_init(glog_ringbuf_t *ringbuf, void *data, size_t size_power_of_two);
size_t glog_rb_put(glog_ringbuf_t *ringbuf, const void *data, size_t length);
size_t glog_rb_get(glog_ringbuf_t *ringbuf, void *buffer, size_t size);
bool glog_rb_clean(glog_ringbuf_t *ringbuf);
size_t glog_rb_length(const glog_ringbuf_t *ringbuf);
size_t glog_rb_avail(const glog_ringbuf_t *ringbuf);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_RINGBUFFER_H */
