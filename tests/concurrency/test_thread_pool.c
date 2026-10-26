#include "thread_pool.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    size_t started;
    size_t finished;
    size_t visits[8];
} Probe;

static void worker(OsThreadPool *pool, size_t index, void *context) {
    Probe *probe = (Probe *)context;
    assert(index < 8);
    assert(os_pool_worker_count(pool) >= index + 1);
    assert(pthread_mutex_lock(&probe->mutex) == 0);
    ++probe->visits[index];
    ++probe->started;
    assert(pthread_cond_broadcast(&probe->ready) == 0);
    assert(pthread_mutex_unlock(&probe->mutex) == 0);
    os_pool_wait_stop(pool);
    assert(os_pool_stop_requested(pool));
    assert(pthread_mutex_lock(&probe->mutex) == 0);
    ++probe->finished;
    assert(pthread_mutex_unlock(&probe->mutex) == 0);
}

static void lifecycle(size_t workers) {
    Probe probe = {0};
    assert(pthread_mutex_init(&probe.mutex, NULL) == 0);
    assert(pthread_cond_init(&probe.ready, NULL) == 0);
    OsThreadPool *pool = NULL;
    assert(os_pool_start(workers, worker, &probe, &pool));
    assert(pool != NULL && os_pool_worker_count(pool) == workers);
    assert(!os_pool_stop_requested(pool));
    assert(pthread_mutex_lock(&probe.mutex) == 0);
    while (probe.started != workers)
        assert(pthread_cond_wait(&probe.ready, &probe.mutex) == 0);
    assert(pthread_mutex_unlock(&probe.mutex) == 0);
    os_pool_request_stop(pool);
    os_pool_request_stop(pool); /* idempotent */
    assert(os_pool_stop_requested(pool));
    os_pool_shutdown(pool);
    assert(probe.started == workers && probe.finished == workers);
    for (size_t i = 0; i < workers; ++i)
        assert(probe.visits[i] == 1);
    assert(pthread_cond_destroy(&probe.ready) == 0);
    assert(pthread_mutex_destroy(&probe.mutex) == 0);
}

int main(void) {
    OsThreadPool *pool = (OsThreadPool *)1;
    assert(!os_pool_start(0, worker, NULL, &pool) && pool == NULL);
    assert(!os_pool_start(4097, worker, NULL, &pool) && pool == NULL);
    assert(!os_pool_start(1, NULL, NULL, &pool) && pool == NULL);
    assert(!os_pool_start(1, worker, NULL, NULL));
    assert(os_pool_worker_count(NULL) == 0);
    assert(os_pool_stop_requested(NULL));
    os_pool_wait_stop(NULL);
    os_pool_request_stop(NULL);
    os_pool_shutdown(NULL);
    lifecycle(1);
    lifecycle(4);
    lifecycle(8);
    return 0;
}
