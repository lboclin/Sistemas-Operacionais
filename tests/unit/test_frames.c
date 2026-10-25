#include "frames.h"

#include <assert.h>
#include <stddef.h>

int main(void) {
    OsFrameCache cache = {0};
    OsFrameAccess access;
    assert(!os_frames_init(&cache, 0, OS_PAGE_FIFO));
    assert(!os_frames_init(&cache, 2, OS_PAGE_OPT));
    assert(os_frames_init(&cache, 2, OS_PAGE_LRU));
    assert(os_frames_access(&cache, (OsPageReference){1, 0}, &access));
    assert(!access.hit && !access.evicted);
    assert(os_frames_access(&cache, (OsPageReference){2, 0}, &access));
    assert(!access.hit && !access.evicted);
    assert(os_frames_access(&cache, (OsPageReference){1, 0}, &access));
    assert(access.hit && !access.evicted);
    assert(os_frames_access(&cache, (OsPageReference){3, 0}, &access));
    assert(!access.hit && access.evicted);
    assert(access.evicted_page.request_id == 2);
    assert(cache.stats.accesses == 4 && cache.stats.hits == 1 &&
           cache.stats.faults == 3 && cache.stats.evictions == 1 && cache.peak_used == 2);
    assert(os_frames_release(&cache, 1) == 1 && cache.used == 1);
    assert(os_frames_release(&cache, 99) == 0);
    os_frames_destroy(&cache);
    assert(cache.entries == NULL);
    assert(os_frames_init(&cache, 1, OS_PAGE_FIFO));
    assert(os_frames_access(&cache, (OsPageReference){1, 1}, &access));
    assert(os_frames_access(&cache, (OsPageReference){2, 1}, &access));
    assert(access.evicted && access.evicted_page.request_id == 1);
    os_frames_destroy(&cache);
    return 0;
}
