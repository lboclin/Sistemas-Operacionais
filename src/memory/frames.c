#include "frames.h"

#include <stdlib.h>

struct OsFrameEntry {
    OsPageReference page;
    uint64_t loaded_at;
    uint64_t accessed_at;
};

static bool same_page(OsPageReference a, OsPageReference b) {
    return a.request_id == b.request_id && a.page_id == b.page_id;
}

static bool cache_valid(const OsFrameCache *cache) {
    return cache != NULL && cache->entries != NULL &&
           cache->capacity != 0 && cache->used <= cache->capacity;
}

bool os_frames_lookup(const OsFrameCache *cache, OsPageReference page,
                      size_t *frame_index) {
    if (!cache_valid(cache) || frame_index == NULL) return false;
    for (size_t i = 0; i < cache->used; ++i) {
        if (same_page(cache->entries[i].page, page)) {
            *frame_index = i;
            return true;
        }
    }
    return false;
}

bool os_frames_page_at(const OsFrameCache *cache, size_t frame_index,
                       OsPageReference *page) {
    if (!cache_valid(cache) || page == NULL || frame_index >= cache->used)
        return false;
    *page = cache->entries[frame_index].page;
    return true;
}

bool os_frames_snapshot_request(const OsFrameCache *cache, uint32_t request_id,
                                OsFrameMapping *mappings, size_t capacity,
                                size_t *count) {
    if (!cache_valid(cache) || count == NULL ||
        (mappings == NULL && capacity != 0)) return false;
    size_t needed = 0;
    for (size_t i = 0; i < cache->used; ++i)
        if (cache->entries[i].page.request_id == request_id) ++needed;
    if (mappings != NULL && capacity < needed) return false;
    if (mappings != NULL) {
        size_t out = 0;
        for (size_t i = 0; i < cache->used; ++i) {
            if (cache->entries[i].page.request_id == request_id) {
                mappings[out++] = (OsFrameMapping){
                    .page_id = cache->entries[i].page.page_id,
                    .frame_index = i
                };
            }
        }
    }
    *count = needed;
    return true;
}

bool os_frames_init(OsFrameCache *cache, size_t capacity, OsPagePolicy policy) {
    if (cache == NULL || capacity == 0 || capacity > SIZE_MAX / sizeof(OsFrameEntry) ||
        (policy != OS_PAGE_FIFO && policy != OS_PAGE_LRU)) return false;
    OsFrameEntry *entries = calloc(capacity, sizeof(*entries));
    if (entries == NULL) return false;
    *cache = (OsFrameCache){.entries = entries, .capacity = capacity, .policy = policy};
    return true;
}

bool os_frames_access(OsFrameCache *cache, OsPageReference page, OsFrameAccess *result) {
    if (cache == NULL || result == NULL || cache->entries == NULL ||
        cache->capacity == 0 || cache->clock == UINT64_MAX ||
        cache->stats.accesses == SIZE_MAX) return false;
    ++cache->clock;
    ++cache->stats.accesses;
    *result = (OsFrameAccess){0};
    for (size_t i = 0; i < cache->used; ++i) {
        if (same_page(cache->entries[i].page, page)) {
            cache->entries[i].accessed_at = cache->clock;
            ++cache->stats.hits;
            result->hit = true;
            return true;
        }
    }
    ++cache->stats.faults;
    size_t index = 0;
    if (cache->used < cache->capacity) {
        index = cache->used++;
        if (cache->used > cache->peak_used) cache->peak_used = cache->used;
    } else {
        for (size_t i = 1; i < cache->used; ++i) {
            const uint64_t chosen = cache->policy == OS_PAGE_FIFO ?
                cache->entries[index].loaded_at : cache->entries[index].accessed_at;
            const uint64_t candidate = cache->policy == OS_PAGE_FIFO ?
                cache->entries[i].loaded_at : cache->entries[i].accessed_at;
            if (candidate < chosen) index = i;
        }
        result->evicted = true;
        result->evicted_page = cache->entries[index].page;
        ++cache->stats.evictions;
    }
    cache->entries[index] = (OsFrameEntry){.page = page, .loaded_at = cache->clock,
                                           .accessed_at = cache->clock};
    return true;
}

size_t os_frames_release(OsFrameCache *cache, uint32_t request_id) {
    if (cache == NULL || cache->entries == NULL) return 0;
    size_t remaining = 0;
    const size_t before = cache->used;
    for (size_t i = 0; i < before; ++i) {
        if (cache->entries[i].page.request_id != request_id)
            cache->entries[remaining++] = cache->entries[i];
    }
    cache->used = remaining;
    return before - remaining;
}

void os_frames_destroy(OsFrameCache *cache) {
    if (cache == NULL) return;
    free(cache->entries);
    *cache = (OsFrameCache){0};
}
