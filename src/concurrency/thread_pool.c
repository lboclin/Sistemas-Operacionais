#include "thread_pool.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    struct OsThreadPool *pool;
    size_t index;
} WorkerArg;

struct OsThreadPool {
    pthread_t *threads;
    WorkerArg *args;
    size_t count;
    size_t started;
    OsWorkerRoutine routine;
    void *context;
    pthread_mutex_t mutex;
    pthread_cond_t stopped;
    bool stopping;
};

static void *run_worker(void *arg) {
    WorkerArg *worker = (WorkerArg *)arg;
    OsThreadPool *pool = worker->pool;
    pool->routine(pool, worker->index, pool->context);
    return NULL;
}

bool os_pool_start(size_t count, OsWorkerRoutine routine, void *context,
                   OsThreadPool **out) {
    if (out == NULL) return false;
    *out = NULL;
    if (count == 0 || count > 4096 || routine == NULL ||
        count > SIZE_MAX / sizeof(pthread_t) ||
        count > SIZE_MAX / sizeof(WorkerArg)) return false;
    OsThreadPool *pool = calloc(1, sizeof(*pool));
    if (pool == NULL) return false;
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool);
        return false;
    }
    if (pthread_cond_init(&pool->stopped, NULL) != 0) {
        (void)pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return false;
    }
    pool->threads = calloc(count, sizeof(*pool->threads));
    pool->args = calloc(count, sizeof(*pool->args));
    if (pool->threads == NULL || pool->args == NULL) {
        os_pool_shutdown(pool);
        return false;
    }
    pool->count = count;
    pool->routine = routine;
    pool->context = context;
    for (size_t i = 0; i < count; ++i) {
        pool->args[i] = (WorkerArg){.pool = pool, .index = i};
        if (pthread_create(&pool->threads[i], NULL, run_worker, &pool->args[i]) != 0) {
            os_pool_shutdown(pool);
            return false;
        }
        ++pool->started;
    }
    *out = pool;
    return true;
}

size_t os_pool_worker_count(const OsThreadPool *pool) {
    return pool == NULL ? 0 : pool->count;
}

bool os_pool_stop_requested(OsThreadPool *pool) {
    if (pool == NULL) return true;
    (void)pthread_mutex_lock(&pool->mutex);
    const bool result = pool->stopping;
    (void)pthread_mutex_unlock(&pool->mutex);
    return result;
}

void os_pool_wait_stop(OsThreadPool *pool) {
    if (pool == NULL) return;
    (void)pthread_mutex_lock(&pool->mutex);
    while (!pool->stopping)
        (void)pthread_cond_wait(&pool->stopped, &pool->mutex);
    (void)pthread_mutex_unlock(&pool->mutex);
}

void os_pool_request_stop(OsThreadPool *pool) {
    if (pool == NULL) return;
    (void)pthread_mutex_lock(&pool->mutex);
    pool->stopping = true;
    (void)pthread_cond_broadcast(&pool->stopped);
    (void)pthread_mutex_unlock(&pool->mutex);
}

void os_pool_shutdown(OsThreadPool *pool) {
    if (pool == NULL) return;
    os_pool_request_stop(pool);
    for (size_t i = 0; i < pool->started; ++i)
        (void)pthread_join(pool->threads[i], NULL);
    (void)pthread_cond_destroy(&pool->stopped);
    (void)pthread_mutex_destroy(&pool->mutex);
    free(pool->args);
    free(pool->threads);
    free(pool);
}
