#define _POSIX_C_SOURCE 200809L
#include "parallel_runtime.h"
#include "session.h"
#include "thread_pool.h"
#include "work_queue.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    OsWorkQueue *queue;
    pthread_mutex_t mutex;
    pthread_cond_t completed_condition;
    const OsTokenJob *expected_jobs; /* borrowed until all acknowledgments */
    bool *acknowledged;              /* protected by mutex */
    size_t expected_count;
    size_t completed_count;
    bool failed;
} OsParallelContext;

static void parallel_worker(OsThreadPool *pool, size_t worker_index, void *opaque) {
    (void)worker_index;
    OsParallelContext *context = (OsParallelContext *)opaque;
    OsWorkItem item;
    while (!os_pool_stop_requested(pool) && os_queue_pop(context->queue, &item)) {
        (void)pthread_mutex_lock(&context->mutex);
        size_t matched = context->expected_count;
        for (size_t i = 0; i < context->expected_count; ++i) {
            if (context->expected_jobs[i].request_id == item.request_id) {
                matched = i;
                break;
            }
        }
        if (item.token_budget != 1 || matched == context->expected_count ||
            context->acknowledged[matched]) {
            context->failed = true;
        } else {
            context->acknowledged[matched] = true;
            ++context->completed_count;
        }
        (void)pthread_cond_signal(&context->completed_condition);
        (void)pthread_mutex_unlock(&context->mutex);
    }
}

/* No session state is shared with workers: only IDs and acknowledgments cross
 * the queue boundary. The coordinator commits token counts after this barrier. */
static bool execute_batch(const OsTokenJob *jobs, size_t count, void *opaque) {
    OsParallelContext *context = (OsParallelContext *)opaque;
    if (jobs == NULL || count == 0) return false;
    (void)pthread_mutex_lock(&context->mutex);
    context->expected_jobs = jobs;
    context->expected_count = count;
    context->completed_count = 0;
    context->failed = false;
    memset(context->acknowledged, 0, count * sizeof(*context->acknowledged));
    (void)pthread_mutex_unlock(&context->mutex);

    for (size_t i = 0; i < count; ++i) {
        if (!os_queue_push(context->queue, (OsWorkItem){
                .request_id = jobs[i].request_id, .token_budget = 1})) return false;
    }
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return false;
    deadline.tv_sec += 15; /* bound a broken worker/queue in CI */
    (void)pthread_mutex_lock(&context->mutex);
    while (!context->failed && context->completed_count < count) {
        const int result = pthread_cond_timedwait(&context->completed_condition,
                                                   &context->mutex, &deadline);
        if (result != 0) {
            context->failed = true;
            break;
        }
    }
    const bool success = !context->failed && context->completed_count == count;
    (void)pthread_mutex_unlock(&context->mutex);
    return success;
}

OsRunStatus os_posix_run(OsRequest *requests, size_t count,
                         const OsVirtualConfig *config, OsRunSummary *summary,
                         OsEventCallback observer, void *observer_context) {
    if (requests == NULL || config == NULL || summary == NULL) return OS_RUN_INVALID;
    OsVirtualSession *session = NULL;
    OsRunStatus status = os_session_create(requests, count, config,
                                           observer, observer_context, &session);
    if (status != OS_RUN_OK) return status;

    OsParallelContext context = {0};
    OsThreadPool *pool = NULL;
    bool mutex_ready = false;
    bool condition_ready = false;
    context.acknowledged = calloc(config->worker_count, sizeof(*context.acknowledged));
    if (context.acknowledged == NULL) { status = OS_RUN_ALLOC_FAILURE; goto cleanup; }
    if (pthread_mutex_init(&context.mutex, NULL) != 0) {
        status = OS_RUN_EXEC_FAILURE; goto cleanup;
    }
    mutex_ready = true;
    if (pthread_cond_init(&context.completed_condition, NULL) != 0) {
        status = OS_RUN_EXEC_FAILURE; goto cleanup;
    }
    condition_ready = true;
    if (!os_queue_create(config->worker_count, &context.queue)) {
        status = OS_RUN_ALLOC_FAILURE; goto cleanup;
    }
    if (!os_pool_start(config->worker_count, parallel_worker, &context, &pool)) {
        status = OS_RUN_EXEC_FAILURE; goto cleanup;
    }
    for (;;) {
        OsSessionSnapshot snapshot;
        if (!os_session_snapshot(session, &snapshot, NULL, 0, NULL, 0)) {
            status = OS_RUN_INVALID; break;
        }
        if (snapshot.finished) {
            if (!os_session_snapshot(session, &snapshot, requests, count, NULL, 0)) {
                status = OS_RUN_INVALID;
            } else {
                *summary = snapshot.metrics;
            }
            break;
        }
        status = os_session_step_with_executor(session, execute_batch, &context);
        if (status != OS_RUN_OK) break;
    }
cleanup:
    /* Close before join: consumers may be blocked in sem_wait. Never free
     * context or queue while workers can still acknowledge a task. */
    os_queue_close(context.queue);
    os_pool_shutdown(pool);
    os_queue_destroy(context.queue);
    if (condition_ready) (void)pthread_cond_destroy(&context.completed_condition);
    if (mutex_ready) (void)pthread_mutex_destroy(&context.mutex);
    free(context.acknowledged);
    os_session_destroy(session);
    return status;
}
