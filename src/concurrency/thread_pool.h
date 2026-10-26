#ifndef OS_THREAD_POOL_H
#define OS_THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>

/* POSIX worker lifecycle, separate from the deterministic virtual engine.
 * A routine runs once per worker and must cooperatively honor shutdown.
 * Queueing and scheduling are intentionally outside this first contract. */
typedef struct OsThreadPool OsThreadPool;
typedef void (*OsWorkerRoutine)(OsThreadPool *pool, size_t worker_index, void *context);

/* Starts count real pthreads. On failure, stops/joins already created threads,
 * frees resources and leaves *out NULL. Maximum count is 4096. */
bool os_pool_start(size_t count, OsWorkerRoutine routine, void *context,
                   OsThreadPool **out);
size_t os_pool_worker_count(const OsThreadPool *pool);

/* All methods below except shutdown may be called by worker routines.
 * wait_stop sleeps on a condition variable; it does not spin. */
bool os_pool_stop_requested(OsThreadPool *pool);
void os_pool_wait_stop(OsThreadPool *pool);
void os_pool_request_stop(OsThreadPool *pool);

/* Requests stop, joins every worker and frees the pool. Must be called from
 * outside the pool, only once, with no concurrent API calls by the owner.
 * A routine that ignores shutdown can prevent join from completing. */
void os_pool_shutdown(OsThreadPool *pool);

#endif
