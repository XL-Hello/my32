#include "queue.h"

#include <stdint.h>
#include <string.h>

#include "platform.h"

glog_queue_status_t glog_queue_init(glog_queue_t *queue,
                                    glog_mem_pool_t *node_pool,
                                    glog_mem_pool_t *control_pool,
                                    size_t max_depth)
{
    if (queue == NULL || node_pool == NULL || control_pool == NULL ||
        max_depth == 0U || max_depth > SIZE_MAX / sizeof(*queue->nodes)) {
        return GLOG_QUEUE_ERR_INVALID_ARG;
    }

    memset(queue, 0, sizeof(*queue));
    queue->nodes = glog_mem_pool_calloc(node_pool, max_depth,
                                        sizeof(*queue->nodes));
    if (queue->nodes == NULL) {
        return GLOG_QUEUE_ERR_NO_MEMORY;
    }

    queue->lock = glog_mutex_create();
    if (queue->lock == NULL) {
        glog_mem_pool_free(node_pool, queue->nodes);
        memset(queue, 0, sizeof(*queue));
        return GLOG_QUEUE_ERR_NO_MEMORY;
    }

    for (size_t index = 0U; index + 1U < max_depth; ++index) {
        queue->nodes[index].next = &queue->nodes[index + 1U];
    }
    queue->free_head = &queue->nodes[0];
    queue->capacity = max_depth;
    queue->node_pool = node_pool;
    queue->control_pool = control_pool;
    queue->initialized = true;
    return GLOG_QUEUE_OK;
}

glog_queue_status_t glog_queue_put(glog_queue_t *queue, void *data)
{
    glog_queue_node_t *node;

    if (queue == NULL || data == NULL) {
        return GLOG_QUEUE_ERR_INVALID_ARG;
    }
    if (!queue->initialized) {
        return GLOG_QUEUE_ERR_NOT_INITIALIZED;
    }
    if (glog_mutex_lock(queue->lock) != GLOG_OK) {
        return GLOG_QUEUE_ERR_BUSY;
    }
    if (queue->free_head == NULL) {
        glog_mutex_unlock(queue->lock);
        return GLOG_QUEUE_ERR_FULL;
    }

    node = queue->free_head;
    queue->free_head = node->next;
    node->data = data;
    node->next = NULL;
    if (queue->tail == NULL) {
        queue->head = node;
    } else {
        queue->tail->next = node;
    }
    queue->tail = node;
    queue->length++;
    glog_mutex_unlock(queue->lock);
    return GLOG_QUEUE_OK;
}

glog_queue_status_t glog_queue_get(glog_queue_t *queue, void **data)
{
    glog_queue_node_t *node;

    if (queue == NULL || data == NULL) {
        return GLOG_QUEUE_ERR_INVALID_ARG;
    }
    *data = NULL;
    if (!queue->initialized) {
        return GLOG_QUEUE_ERR_NOT_INITIALIZED;
    }
    if (glog_mutex_lock(queue->lock) != GLOG_OK) {
        return GLOG_QUEUE_ERR_BUSY;
    }
    if (queue->head == NULL) {
        glog_mutex_unlock(queue->lock);
        return GLOG_QUEUE_ERR_EMPTY;
    }

    node = queue->head;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->length--;
    *data = node->data;
    node->data = NULL;
    node->next = queue->free_head;
    queue->free_head = node;
    glog_mutex_unlock(queue->lock);
    return GLOG_QUEUE_OK;
}

glog_queue_status_t glog_queue_deinit(glog_queue_t *queue)
{
    glog_mem_pool_t *node_pool;
    glog_queue_node_t *nodes;

    if (queue == NULL) {
        return GLOG_QUEUE_ERR_INVALID_ARG;
    }
    if (!queue->initialized) {
        return GLOG_QUEUE_ERR_NOT_INITIALIZED;
    }
    if (glog_mutex_lock(queue->lock) != GLOG_OK) {
        return GLOG_QUEUE_ERR_BUSY;
    }
    if (queue->length != 0U) {
        glog_mutex_unlock(queue->lock);
        return GLOG_QUEUE_ERR_BUSY;
    }

    queue->initialized = false;
    glog_mutex_unlock(queue->lock);
    glog_mutex_destroy(queue->lock);
    node_pool = queue->node_pool;
    nodes = queue->nodes;
    memset(queue, 0, sizeof(*queue));
    glog_mem_pool_free(node_pool, nodes);
    return GLOG_QUEUE_OK;
}

size_t glog_queue_length(glog_queue_t *queue)
{
    size_t length;

    if (queue == NULL || !queue->initialized ||
        glog_mutex_lock(queue->lock) != GLOG_OK) {
        return 0U;
    }
    length = queue->length;
    glog_mutex_unlock(queue->lock);
    return length;
}

bool glog_queue_empty(glog_queue_t *queue)
{
    return glog_queue_length(queue) == 0U;
}
