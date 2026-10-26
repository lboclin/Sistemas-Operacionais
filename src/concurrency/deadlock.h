#ifndef OS_DEADLOCK_H
#define OS_DEADLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Exclusive-resource wait-for graph. Each process may wait for one resource;
 * each resource has at most one owner. No locks are taken by this model.
 * Call from one coordinator thread, not concurrently. */
typedef struct OsWaitGraph OsWaitGraph;
#define OS_DEADLOCK_NONE SIZE_MAX

bool os_deadlock_create(size_t process_count, size_t resource_count,
                        OsWaitGraph **out);
bool os_deadlock_set_owner(OsWaitGraph *graph, size_t resource, size_t process);
bool os_deadlock_set_wait(OsWaitGraph *graph, size_t process, size_t resource);

/* Finds one deterministic cycle in the derived wait-for graph. Returns true
 * for valid calls, even when no cycle exists (then count=0). NULL cycle with
 * capacity=0 queries the required length. Insufficient capacity fails without
 * modifying count or the caller's cycle buffer. */
bool os_deadlock_find_cycle(const OsWaitGraph *graph, size_t *cycle,
                            size_t capacity, size_t *count);
/* Nonblocking prevention: grant only a free resource whose index is higher
 * than every resource already held by this process. Busy or out-of-order
 * requests fail without changing ownership/waits. This models total-order
 * acquisition; it does not acquire OS mutexes or block real threads. */
bool os_deadlock_acquire_ordered(OsWaitGraph *graph, size_t process, size_t resource);
/* Release all resources and clear the process wait edge. */
bool os_deadlock_release_all(OsWaitGraph *graph, size_t process, size_t *released);
/* Controlled recovery of one cycle: abort the highest-index process in the
 * cycle, releasing its resources and wait edge. If no cycle exists, returns
 * victim=NONE and released=0. Never cancels a pthread. */
bool os_deadlock_recover_one(OsWaitGraph *graph, size_t *victim, size_t *released);
void os_deadlock_destroy(OsWaitGraph *graph);

#endif
