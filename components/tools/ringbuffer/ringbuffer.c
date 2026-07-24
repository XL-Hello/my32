#include "ringbuffer.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
struct ring_buffer {
    uint8_t *data;
    size_t head;
    size_t tail;
    size_t size;
    size_t overflow_cnt;

    portMUX_TYPE lock;
    bool lock_enabled;
};

static size_t s_rb_get_used(const ring_buffer_t *rb);
static size_t s_rb_get_free(const ring_buffer_t *rb);

ring_buffer_t *rb_init(size_t size, bool need_lock)
{
    if (size < 2) {
        return NULL;
    }

    ring_buffer_t *rb = calloc(1, sizeof(*rb));
    if (!rb) {
        return NULL;
    }

    rb->data = malloc(size);
    if (!rb->data) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    rb->lock_enabled = need_lock;
    return rb;
}

static void rb_lock_take(ring_buffer_t *rb)
{
    if (rb->lock_enabled) {
        portENTER_CRITICAL(&rb->lock);
    }
}

static void rb_lock_give(ring_buffer_t *rb)
{
    if (rb->lock_enabled) {
        portEXIT_CRITICAL(&rb->lock);
    }
}

bool rb_clear(ring_buffer_t *rb)
{
    if (!rb) {
        return false;
    }

    rb_lock_take(rb);
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_cnt = 0;
    rb_lock_give(rb);
    return true;
}

bool rb_destroy(ring_buffer_t *rb)
{
    if (!rb) {
        return false;
    }

    rb_lock_take(rb);
    free(rb->data);
    rb_lock_give(rb);
    free(rb);
    return true;
}

bool rb_is_empty(ring_buffer_t *rb)
{
    if (!rb) {
        return true;
    }

    rb_lock_take(rb);
    bool val = rb->head == rb->tail;
    rb_lock_give(rb);
    return val;
}

bool rb_is_full(ring_buffer_t *rb)
{
    if (!rb) {
        return false;
    }

    rb_lock_take(rb);
    size_t next = rb->head + 1;
    if (next >= rb->size) {
        next = 0;
    }
    bool val = (next == rb->tail);
    rb_lock_give(rb);
    return val;
}

size_t rb_get_used(ring_buffer_t *rb)
{
    if (!rb) {
        return 0;
    }

    rb_lock_take(rb);
    size_t used_space = s_rb_get_used(rb);
    rb_lock_give(rb);
    return used_space;
}

size_t rb_get_free(ring_buffer_t *rb)
{
    if (!rb) {
        return 0;
    }

    rb_lock_take(rb);
    size_t free_space = s_rb_get_free(rb);
    rb_lock_give(rb);
    return free_space;
}

size_t rb_get_overflow_count(ring_buffer_t *rb)
{
    if (!rb) {
        return 0;
    }

    rb_lock_take(rb);
    size_t overflow_count = rb->overflow_cnt;
    rb_lock_give(rb);
    return overflow_count;
}

static size_t s_rb_get_used(const ring_buffer_t *rb)
{
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    }

    return rb->size - (rb->tail - rb->head);
}

static size_t s_rb_get_free(const ring_buffer_t *rb)
{
    return rb->size - s_rb_get_used(rb) - 1;
}

int rb_put(ring_buffer_t *rb, const uint8_t *data, size_t len)
{
    if (!rb || !data) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    rb_lock_take(rb);
    size_t free_space = s_rb_get_free(rb);
    size_t wlen = free_space >= len ? len : free_space;
    if (wlen < len) {
        rb->overflow_cnt++;
    }
    for (size_t i = 0; i < wlen; i++) {
        rb->data[rb->head] = data[i];
        rb->head++;
        if (rb->head >= rb->size) {
            rb->head = 0;
        }
    }
    rb_lock_give(rb);
    return wlen;
}

int rb_put_from_isr(ring_buffer_t *rb, const uint8_t *data, size_t len)
{
    if (!rb || !data) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    if (rb->lock_enabled) {
        portENTER_CRITICAL_ISR(&rb->lock);
    }
    size_t free_space = s_rb_get_free(rb);
    size_t wlen = free_space >= len ? len : free_space;
    if (wlen < len) {
        rb->overflow_cnt++;
    }
    for (size_t i = 0; i < wlen; i++) {
        rb->data[rb->head] = data[i];
        rb->head++;
        if (rb->head >= rb->size) {
            rb->head = 0;
        }
    }
    if (rb->lock_enabled) {
        portEXIT_CRITICAL_ISR(&rb->lock);
    }
    return wlen;
}

int rb_get(ring_buffer_t *rb, uint8_t *data, size_t len)
{
    if (!rb || !data) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    rb_lock_take(rb);
    size_t use_space = s_rb_get_used(rb);
    size_t rlen = use_space >= len ? len : use_space;
    for (size_t i = 0; i < rlen; i++) {
        data[i] = rb->data[rb->tail];
        rb->tail++;
        if (rb->tail >= rb->size) {
            rb->tail = 0;
        }
    }
    rb_lock_give(rb);
    return rlen;
}
