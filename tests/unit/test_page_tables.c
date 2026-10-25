#include "frames.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void reference(OsFrameCache *cache, uint32_t req, uint32_t page) {
    OsFrameAccess result;
    assert(os_frames_access(cache, (OsPageReference){req, page}, &result));
}

static void per_request_mapping(void) {
    OsFrameCache cache = {0};
    assert(os_frames_init(&cache, 4, OS_PAGE_LRU));
    reference(&cache, 10, 0);  /* frame 0 */
    reference(&cache, 20, 0);  /* frame 1: same page ID, distinct request */
    reference(&cache, 10, 1);  /* frame 2 */
    reference(&cache, 30, 0);  /* frame 3 */

    size_t slot = SIZE_MAX;
    assert(os_frames_lookup(&cache, (OsPageReference){10, 0}, &slot) && slot == 0);
    assert(os_frames_lookup(&cache, (OsPageReference){20, 0}, &slot) && slot == 1);
    assert(!os_frames_lookup(&cache, (OsPageReference){20, 1}, &slot) && slot == 1);
    assert(!os_frames_lookup(NULL, (OsPageReference){10, 0}, &slot));
    assert(!os_frames_lookup(&cache, (OsPageReference){10, 0}, NULL));

    const OsPageStats before = cache.stats;
    const uint64_t clock = cache.clock;
    size_t count = SIZE_MAX;
    assert(os_frames_snapshot_request(&cache, 10, NULL, 0, &count) && count == 2);
    assert(cache.clock == clock && memcmp(&before, &cache.stats, sizeof before) == 0);
    OsFrameMapping table[2] = {{UINT32_MAX, SIZE_MAX}, {UINT32_MAX, SIZE_MAX}};
    assert(os_frames_snapshot_request(&cache, 10, table, 2, &count) && count == 2);
    assert(table[0].page_id == 0 && table[0].frame_index == 0);
    assert(table[1].page_id == 1 && table[1].frame_index == 2);
    assert(os_frames_snapshot_request(&cache, 20, table, 2, &count) && count == 1);
    assert(table[0].page_id == 0 && table[0].frame_index == 1);
    assert(os_frames_snapshot_request(&cache, 99, NULL, 0, &count) && count == 0);

    /* Insufficient capacity and invalid arguments cannot modify outputs. */
    OsFrameMapping untouched[2] = {{33, 55}, {44, 66}};
    OsFrameMapping copy[2];
    memcpy(copy, untouched, sizeof copy);
    count = 777;
    assert(!os_frames_snapshot_request(&cache, 10, untouched, 1, &count));
    assert(count == 777 && memcmp(copy, untouched, sizeof copy) == 0);
    assert(!os_frames_snapshot_request(&cache, 10, NULL, 1, &count));
    assert(!os_frames_snapshot_request(&cache, 10, untouched, 2, NULL));
    assert(!os_frames_snapshot_request(NULL, 10, untouched, 2, &count));
    assert(count == 777 && memcmp(copy, untouched, sizeof copy) == 0);

    /* Evict only the oldest LRU page; all other mappings survive. */
    reference(&cache, 10, 0); /* make frame 0 newest */
    reference(&cache, 40, 0); /* evicts request 20's page at frame 1 */
    assert(!os_frames_lookup(&cache, (OsPageReference){20, 0}, &slot));
    assert(os_frames_lookup(&cache, (OsPageReference){40, 0}, &slot) && slot == 1);
    assert(os_frames_snapshot_request(&cache, 10, table, 2, &count) && count == 2);
    assert(os_frames_release(&cache, 10) == 2);
    assert(!os_frames_lookup(&cache, (OsPageReference){10, 0}, &slot));
    assert(os_frames_lookup(&cache, (OsPageReference){40, 0}, &slot) && slot == 0);
    assert(os_frames_lookup(&cache, (OsPageReference){30, 0}, &slot) && slot == 1);
    assert(os_frames_snapshot_request(&cache, 40, table, 2, &count) &&
           count == 1 && table[0].frame_index == 0);
    assert(os_frames_release(&cache, 10) == 0);
    assert(os_frames_release(&cache, 40) == 1);
    assert(os_frames_release(&cache, 30) == 1 && cache.used == 0);
    assert(os_frames_snapshot_request(&cache, 10, NULL, 0, &count) && count == 0);
    os_frames_destroy(&cache);
    assert(!os_frames_snapshot_request(&cache, 10, NULL, 0, &count));
}

static void fifo_and_single_frame(void) {
    OsFrameCache cache = {0};
    assert(os_frames_init(&cache, 1, OS_PAGE_FIFO));
    reference(&cache, 1, 9);
    reference(&cache, 2, 9);
    size_t slot = SIZE_MAX;
    assert(!os_frames_lookup(&cache, (OsPageReference){1, 9}, &slot));
    assert(os_frames_lookup(&cache, (OsPageReference){2, 9}, &slot) && slot == 0);
    assert(os_frames_release(&cache, 1) == 0 && cache.used == 1);
    os_frames_destroy(&cache);
}

int main(void) {
    per_request_mapping();
    fifo_and_single_frame();
    return 0;
}
