#include "pager.h"

#include <stdlib.h>

typedef struct {
    OsPageReference ref;
    size_t loaded_at;
    size_t accessed_at;
} Frame;

static bool same_page(OsPageReference a, OsPageReference b) {
    return a.request_id == b.request_id && a.page_id == b.page_id;
}

bool os_pager_replay(const OsPageReference *references, size_t count,
                     size_t frame_capacity, OsPagePolicy policy, OsPageStats *stats) {
    if (stats == NULL || (count != 0 && references == NULL) || frame_capacity == 0 ||
        frame_capacity > SIZE_MAX / sizeof(Frame) ||
        policy < OS_PAGE_FIFO || policy > OS_PAGE_OPT) return false;
    Frame *frames = calloc(frame_capacity, sizeof(*frames));
    if (frames == NULL) return false;
    OsPageStats result = {0};
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        ++result.accesses;
        size_t hit = used;
        for (size_t j = 0; j < used; ++j)
            if (same_page(frames[j].ref, references[i])) { hit = j; break; }
        if (hit != used) {
            ++result.hits;
            frames[hit].accessed_at = i;
            continue;
        }
        ++result.faults;
        size_t victim;
        if (used < frame_capacity) {
            victim = used++;
        } else {
            ++result.evictions;
            victim = 0;
            if (policy == OS_PAGE_OPT) {
                size_t farthest = 0;
                for (size_t j = 0; j < used; ++j) {
                    size_t next = count;
                    for (size_t k = i + 1; k < count; ++k)
                        if (same_page(frames[j].ref, references[k])) { next = k; break; }
                    if (next > farthest) { farthest = next; victim = j; }
                }
            } else {
                for (size_t j = 1; j < used; ++j) {
                    size_t t = policy == OS_PAGE_FIFO ? frames[j].loaded_at : frames[j].accessed_at;
                    size_t old = policy == OS_PAGE_FIFO ? frames[victim].loaded_at : frames[victim].accessed_at;
                    if (t < old) victim = j;
                }
            }
        }
        frames[victim] = (Frame){.ref = references[i], .loaded_at = i, .accessed_at = i};
    }
    *stats = result;
    free(frames);
    return true;
}
