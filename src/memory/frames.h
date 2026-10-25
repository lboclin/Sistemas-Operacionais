#ifndef OS_FRAMES_H
#define OS_FRAMES_H

#include "pager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Online resident-frame cache. OPT is intentionally unavailable: its input
 * requires future references and is supported only by os_pager_replay. */
typedef struct OsFrameEntry OsFrameEntry;
typedef struct {
    OsFrameEntry *entries; /* private: do not access outside frames.c */
    size_t capacity;
    size_t used;
    size_t peak_used;
    uint64_t clock;
    OsPagePolicy policy;
    OsPageStats stats;
} OsFrameCache;

typedef struct {
    bool hit;
    bool evicted;
    OsPageReference evicted_page;
} OsFrameAccess;

/* A request-scoped view of the resident page table. Frame indices refer to
 * the cache's current physical slots; eviction or release may move mappings. */
typedef struct {
    uint32_t page_id;
    size_t frame_index;
} OsFrameMapping;

/* Look up one logical (request_id, page_id) without touching LRU or metrics.
 * On a miss or invalid input, frame_index is left unchanged. */
bool os_frames_lookup(const OsFrameCache *cache, OsPageReference page,
                      size_t *frame_index);

/* List resident pages for one request in ascending physical frame order.
 * count is required. Passing mappings=NULL with capacity=0 queries the count
 * only. Insufficient capacity or invalid arguments leave all outputs intact. */
bool os_frames_snapshot_request(const OsFrameCache *cache, uint32_t request_id,
                                OsFrameMapping *mappings, size_t capacity,
                                size_t *count);

bool os_frames_init(OsFrameCache *cache, size_t capacity, OsPagePolicy policy);
/* Non-throwing; false indicates an invalid cache, bad result or clock overflow. */
bool os_frames_access(OsFrameCache *cache, OsPageReference page, OsFrameAccess *result);
/* Release all resident pages belonging to a completed request. */
size_t os_frames_release(OsFrameCache *cache, uint32_t request_id);
void os_frames_destroy(OsFrameCache *cache);

#endif
