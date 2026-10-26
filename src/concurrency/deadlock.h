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
void os_deadlock_destroy(OsWaitGraph *graph);

#endif
