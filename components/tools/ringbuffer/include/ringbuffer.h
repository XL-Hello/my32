#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 线程安全的字节环形缓冲区。
 *
 * 缓冲区使用一个空闲字节区分“空”和“满”，因此实际可写容量为
 * `rb_init()` 传入容量减一。
 */
typedef struct ring_buffer ring_buffer_t;

/**
 * @brief 创建环形缓冲区。
 *
 * @param size 缓冲区总容量，至少为 2 字节。
 * @param need_lock 是否需要互斥锁保护。
 * @return 成功时返回缓冲区句柄；分配或互斥锁创建失败时返回 NULL。
 */
ring_buffer_t *rb_init(size_t size, bool need_lock);

/**
 * @brief 释放环形缓冲区及其关联资源。
 *
 * 调用后句柄失效。调用方须确保没有其他任务正在使用该缓冲区。
 *
 * @param rb 缓冲区句柄。
 * @return 成功释放返回 true；参数为 NULL 时返回 false。
 */
bool rb_destroy(ring_buffer_t *rb);

/** @brief 清空缓冲区并清零溢出计数。 */
bool rb_clear(ring_buffer_t *rb);

/** @brief 判断缓冲区是否为空。NULL 句柄视为空。 */
bool rb_is_empty(ring_buffer_t *rb);

/** @brief 判断缓冲区是否已满。 */
bool rb_is_full(ring_buffer_t *rb);

/** @brief 获取当前可读取的字节数。 */
size_t rb_get_used(ring_buffer_t *rb);

/** @brief 获取当前可写入的字节数。 */
size_t rb_get_free(ring_buffer_t *rb);

/** @brief 获取因空间不足而发生的写入溢出次数。 */
size_t rb_get_overflow_count(ring_buffer_t *rb);

/**
 * @brief 向缓冲区写入数据。
 *
 * 空间不足时仅写入可容纳的数据，并将溢出计数加一。
 *
 * @return 实际写入字节数；参数无效返回 -1。
 */
int rb_put(ring_buffer_t *rb, const uint8_t *data, size_t len);

/**
 * @brief 在中断服务程序中向缓冲区写入数据。
 *
 * 与 @ref rb_put 功能相同，但仅可从 ISR 上下文调用；不会阻塞。任务
 * 上下文与 ISR 上下文可安全地并发使用同一缓冲区。
 *
 * @return 实际写入字节数；参数无效返回 -1。
 */
int rb_put_from_isr(ring_buffer_t *rb, const uint8_t *data, size_t len);

/**
 * @brief 从缓冲区读取数据。
 *
 * @return 实际读取字节数；参数无效返回 -1。
 */
int rb_get(ring_buffer_t *rb, uint8_t *data, size_t len);
