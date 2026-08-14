#include "ringbuffer.h"

#include <limits.h>
#include <string.h>

static inline size_t glog_rb_min(size_t first, size_t second)
{
    return first < second ? first : second;
}

static bool glog_rb_is_valid(const glog_ringbuf_t *ringbuf)
{
    return ringbuf != NULL && ringbuf->initialized &&
           ringbuf->data != NULL && ringbuf->mask != 0U;
}

bool glog_rb_init(glog_ringbuf_t *ringbuf,
                  void *data,
                  size_t size_power_of_two)
{
    if (ringbuf == NULL || data == NULL || size_power_of_two < 2U ||
        size_power_of_two > (size_t)UINT16_MAX + 1U ||
        (size_power_of_two & (size_power_of_two - 1U)) != 0U) {
        return false;
    }

    ringbuf->head = 0U;
    ringbuf->tail = 0U;
    ringbuf->data = data;
    ringbuf->mask = (uint16_t)(size_power_of_two - 1U);
    ringbuf->initialized = true;
    return true;
}

size_t glog_rb_length(const glog_ringbuf_t *ringbuf)
{
    if (!glog_rb_is_valid(ringbuf)) {
        return 0U;
    }
    return (ringbuf->tail - ringbuf->head) & ringbuf->mask;
}

size_t glog_rb_avail(const glog_ringbuf_t *ringbuf)
{
    if (!glog_rb_is_valid(ringbuf)) {
        return 0U;
    }
    return ringbuf->mask - glog_rb_length(ringbuf);
}

size_t glog_rb_put(glog_ringbuf_t *ringbuf, const void *data, size_t length)
{
    const uint8_t *source = data;
    size_t first;

    if (!glog_rb_is_valid(ringbuf) || (data == NULL && length != 0U) ||
        glog_rb_avail(ringbuf) < length) {
        return 0U;
    }
    if (length == 0U) {
        return 0U;
    }

    first = glog_rb_min(length, (size_t)ringbuf->mask - ringbuf->tail + 1U);
    memcpy(ringbuf->data + ringbuf->tail, source, first);
    memcpy(ringbuf->data, source + first, length - first);
    ringbuf->tail = (uint16_t)((ringbuf->tail + length) & ringbuf->mask);
    return length;
}

size_t glog_rb_get(glog_ringbuf_t *ringbuf, void *buffer, size_t size)
{
    uint8_t *destination = buffer;
    size_t length;
    size_t first;

    if (!glog_rb_is_valid(ringbuf) || (buffer == NULL && size != 0U)) {
        return 0U;
    }
    length = glog_rb_min(size, glog_rb_length(ringbuf));
    if (length == 0U) {
        return 0U;
    }

    first = glog_rb_min(length, (size_t)ringbuf->mask - ringbuf->head + 1U);
    memcpy(destination, ringbuf->data + ringbuf->head, first);
    memcpy(destination + first, ringbuf->data, length - first);
    ringbuf->head = (uint16_t)((ringbuf->head + length) & ringbuf->mask);
    return length;
}

bool glog_rb_clean(glog_ringbuf_t *ringbuf)
{
    if (!glog_rb_is_valid(ringbuf)) {
        return false;
    }
    ringbuf->head = 0U;
    ringbuf->tail = 0U;
    return true;
}
