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

bool os_frames_init(OsFrameCache *cache, size_t capacity, OsPagePolicy policy);
/* Non-throwing; false indicates an invalid cache, bad result or clock overflow. */
bool os_frames_access(OsFrameCache *cache, OsPageReference page, OsFrameAccess *result);
/* Release all resident pages belonging to a completed request. */
size_t os_frames_release(OsFrameCache *cache, uint32_t request_id);
void os_frames_destroy(OsFrameCache *cache);

#endif
