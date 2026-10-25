#include "pager.h"
#include <assert.h>
#include <stddef.h>

int main(void) {
    const unsigned ids[] = {7,0,1,2,0,3,0,4,2,3,0,3,2};
    OsPageReference refs[sizeof(ids) / sizeof(ids[0])];
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
        refs[i] = (OsPageReference){.request_id = 1, .page_id = ids[i]};
    OsPageStats fifo, lru, opt;
    const size_t n = sizeof refs / sizeof refs[0];
    assert(os_pager_replay(refs, n, 3, OS_PAGE_FIFO, &fifo));
    assert(os_pager_replay(refs, n, 3, OS_PAGE_LRU, &lru));
    assert(os_pager_replay(refs, n, 3, OS_PAGE_OPT, &opt));
    assert(fifo.accesses == n && lru.accesses == n && opt.accesses == n);
    assert(opt.faults <= fifo.faults && opt.faults <= lru.faults);
    assert(fifo.faults == 10 && lru.faults == 9 && opt.faults == 7);
    assert(fifo.hits + fifo.faults == n);
    assert(fifo.evictions == fifo.faults - 3);
    assert(!os_pager_replay(refs, n, 0, OS_PAGE_FIFO, &fifo));
    assert(!os_pager_replay(NULL, n, 3, OS_PAGE_FIFO, &fifo));
    assert(os_pager_replay(NULL, 0, 3, OS_PAGE_FIFO, &fifo));
    assert(fifo.faults == 0 && fifo.accesses == 0);

    const OsPageReference distinct[] = {{1,0},{2,0},{1,0}};
    assert(os_pager_replay(distinct, 3, 1, OS_PAGE_LRU, &lru));
    assert(lru.faults == 3);
    return 0;
}
