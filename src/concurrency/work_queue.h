#ifndef OS_WORK_QUEUE_H
#define OS_WORK_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct OsWorkQueue OsWorkQueue;

/* Value-only queue item: the queue never owns a PCB or caller pointer.
 * A future runtime adapter maps request_id to a validated PCB. */
typedef struct {
    uint32_t request_id;
    uint32_t token_budget;
} OsWorkItem;

/* Bounded FIFO. pthread mutex protects all shared state; POSIX semaphores
 * count available slots and ready items. */
bool os_queue_create(size_t capacity, OsWorkQueue **out);
bool os_queue_push(OsWorkQueue *queue, OsWorkItem item);
bool os_queue_pop(OsWorkQueue *queue, OsWorkItem *out);

/* Cooperative cancellation: close rejects new pushes, wakes blocked
 * producers/consumers and permits consumers to drain queued work.
 * Safe to call repeatedly, including while other threads use the queue. */
void os_queue_close(OsWorkQueue *queue);
size_t os_queue_size(OsWorkQueue *queue);
bool os_queue_is_closed(OsWorkQueue *queue);

/* Must be called only after all producers and consumers have joined.
 * Do not destroy a queue while any thread may still use its pointer. */
void os_queue_destroy(OsWorkQueue *queue);

#endif
