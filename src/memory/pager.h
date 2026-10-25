#ifndef OS_PAGER_H
#define OS_PAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { OS_PAGE_FIFO, OS_PAGE_LRU, OS_PAGE_OPT } OsPagePolicy;

typedef struct {
    uint32_t request_id;
    uint32_t page_id;
} OsPageReference;

typedef struct {
    size_t accesses;
    size_t hits;
    size_t faults;
    size_t evictions;
} OsPageStats;

/* Offline replay of a known reference trace. OPT uses future references as an
 * oracle and is not suitable for online production page replacement. */
bool os_pager_replay(const OsPageReference *references, size_t count,
                     size_t frame_capacity, OsPagePolicy policy, OsPageStats *stats);

#endif
