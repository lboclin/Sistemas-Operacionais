#include "deadlock.h"

#include <assert.h>
#include <stddef.h>

static void cycles(void) {
    OsWaitGraph *g = NULL;
    assert(os_deadlock_create(4, 4, &g));
    size_t count = 99;
    assert(os_deadlock_find_cycle(g, NULL, 0, &count) && count == 0);
    assert(os_deadlock_set_owner(g, 0, 0));
    assert(os_deadlock_set_owner(g, 1, 1));
    assert(os_deadlock_set_owner(g, 2, 2));
    assert(os_deadlock_set_wait(g, 0, 1));
    assert(os_deadlock_set_wait(g, 1, 2));
    assert(os_deadlock_find_cycle(g, NULL, 0, &count) && count == 0);
    assert(os_deadlock_set_wait(g, 2, 0));
    assert(os_deadlock_find_cycle(g, NULL, 0, &count) && count == 3);
    size_t cycle[4] = {77, 77, 77, 77};
    count = 99;
    assert(!os_deadlock_find_cycle(g, cycle, 2, &count));
    assert(count == 99 && cycle[0] == 77 && cycle[1] == 77);
    assert(os_deadlock_find_cycle(g, cycle, 4, &count) && count == 3);
    assert(cycle[0] == 0 && cycle[1] == 1 && cycle[2] == 2);
    assert(os_deadlock_set_owner(g, 0, OS_DEADLOCK_NONE));
    assert(os_deadlock_find_cycle(g, cycle, 4, &count) && count == 0);
    assert(os_deadlock_set_owner(g, 0, 0));
    assert(os_deadlock_set_wait(g, 2, OS_DEADLOCK_NONE));
    assert(os_deadlock_set_wait(g, 0, 0));
    assert(os_deadlock_find_cycle(g, cycle, 4, &count) && count == 1 && cycle[0] == 0);
    assert(os_deadlock_set_wait(g, 0, OS_DEADLOCK_NONE));
    assert(os_deadlock_find_cycle(g, cycle, 4, &count) && count == 0);
    assert(!os_deadlock_set_owner(g, 4, 0));
    assert(!os_deadlock_set_owner(g, 0, 4));
    assert(!os_deadlock_set_wait(g, 4, 0));
    assert(!os_deadlock_set_wait(g, 0, 4));
    os_deadlock_destroy(g);
}

int main(void) {
    OsWaitGraph *g = (OsWaitGraph *)1;
    assert(!os_deadlock_create(0, 2, &g) && g == NULL);
    assert(!os_deadlock_create(2, 0, &g) && g == NULL);
    assert(!os_deadlock_create(129, 2, &g) && g == NULL);
    assert(!os_deadlock_create(2, 129, &g) && g == NULL);
    assert(!os_deadlock_create(2, 2, NULL));
    assert(!os_deadlock_find_cycle(NULL, NULL, 0, NULL));
    os_deadlock_destroy(NULL);
    cycles();
    return 0;
}
