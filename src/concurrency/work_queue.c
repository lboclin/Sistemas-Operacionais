#include "work_queue.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>

struct OsWorkQueue {
    OsWorkItem *items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    size_t waiting_push;
    size_t waiting_pop;
    bool closed;
    pthread_mutex_t mutex;
    sem_t slots;
    sem_t ready;
};

static bool wait_sem(sem_t *sem) {
    for (;;) {
        if (sem_wait(sem) == 0) return true;
        if (errno != EINTR) return false;
    }
}

bool os_queue_create(size_t capacity, OsWorkQueue **out) {
    if (out == NULL) return false;
    *out = NULL;
    if (capacity == 0 || capacity > SIZE_MAX / sizeof(OsWorkItem) ||
        capacity > 65536) return false;
    OsWorkQueue *q = calloc(1, sizeof(*q));
    if (q == NULL) return false;
    q->items = calloc(capacity, sizeof(*q->items));
    if (q->items == NULL) { free(q); return false; }
    q->capacity = capacity;
    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        free(q->items);
        free(q);
        return false;
    }
    if (sem_init(&q->slots, 0, (unsigned)capacity) != 0) {
        (void)pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        return false;
    }
    if (sem_init(&q->ready, 0, 0) != 0) {
        (void)sem_destroy(&q->slots);
        (void)pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        return false;
    }
    *out = q;
    return true;
}

bool os_queue_push(OsWorkQueue *q, OsWorkItem item) {
    if (q == NULL || item.token_budget == 0) return false;
    (void)pthread_mutex_lock(&q->mutex);
    if (q->closed) {
        (void)pthread_mutex_unlock(&q->mutex);
        return false;
    }
    ++q->waiting_push;
    (void)pthread_mutex_unlock(&q->mutex);
    const bool acquired = wait_sem(&q->slots);
    (void)pthread_mutex_lock(&q->mutex);
    --q->waiting_push;
    if (!acquired || q->closed) {
        (void)pthread_mutex_unlock(&q->mutex);
        if (acquired) (void)sem_post(&q->slots);
        return false;
    }
    q->items[q->tail] = item;
    q->tail = q->tail + 1 == q->capacity ? 0 : q->tail + 1;
    ++q->count;
    (void)pthread_mutex_unlock(&q->mutex);
    (void)sem_post(&q->ready);
    return true;
}

bool os_queue_pop(OsWorkQueue *q, OsWorkItem *out) {
    if (q == NULL || out == NULL) return false;
    (void)pthread_mutex_lock(&q->mutex);
    if (q->closed && q->count == 0) {
        (void)pthread_mutex_unlock(&q->mutex);
        return false;
    }
    ++q->waiting_pop;
    (void)pthread_mutex_unlock(&q->mutex);
    const bool acquired = wait_sem(&q->ready);
    (void)pthread_mutex_lock(&q->mutex);
    --q->waiting_pop;
    if (!acquired || q->count == 0) {
        (void)pthread_mutex_unlock(&q->mutex);
        return false;
    }
    *out = q->items[q->head];
    q->head = q->head + 1 == q->capacity ? 0 : q->head + 1;
    --q->count;
    (void)pthread_mutex_unlock(&q->mutex);
    (void)sem_post(&q->slots);
    return true;
}

void os_queue_close(OsWorkQueue *q) {
    if (q == NULL) return;
    (void)pthread_mutex_lock(&q->mutex);
    if (!q->closed) {
        q->closed = true;
        for (size_t i = 0; i < q->waiting_push; ++i)
            (void)sem_post(&q->slots);
        for (size_t i = 0; i < q->waiting_pop; ++i)
            (void)sem_post(&q->ready);
    }
    (void)pthread_mutex_unlock(&q->mutex);
}

size_t os_queue_size(OsWorkQueue *q) {
    if (q == NULL) return 0;
    (void)pthread_mutex_lock(&q->mutex);
    const size_t size = q->count;
    (void)pthread_mutex_unlock(&q->mutex);
    return size;
}

bool os_queue_is_closed(OsWorkQueue *q) {
    if (q == NULL) return true;
    (void)pthread_mutex_lock(&q->mutex);
    const bool closed = q->closed;
    (void)pthread_mutex_unlock(&q->mutex);
    return closed;
}

void os_queue_destroy(OsWorkQueue *q) {
    if (q == NULL) return;
    os_queue_close(q);
    (void)sem_destroy(&q->ready);
    (void)sem_destroy(&q->slots);
    (void)pthread_mutex_destroy(&q->mutex);
    free(q->items);
    free(q);
}
