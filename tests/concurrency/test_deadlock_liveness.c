#include "deadlock.h"

#include <assert.h>
#include <stddef.h>

static void prevention(void) {
    OsWaitGraph *g = NULL;
    assert(os_deadlock_create(3, 3, &g));
    assert(os_deadlock_acquire_ordered(g, 0, 1));
    assert(!os_deadlock_acquire_ordered(g, 0, 0)); /* reverse order */
    assert(!os_deadlock_acquire_ordered(g, 1, 1)); /* resource busy */
    assert(os_deadlock_acquire_ordered(g, 0, 2));
    size_t length = 99;
    assert(os_deadlock_find_cycle(g, NULL, 0, &length) && length == 0);
    size_t released = 99;
    assert(os_deadlock_release_all(g, 0, &released) && released == 2);
    assert(os_deadlock_acquire_ordered(g, 1, 0));
    assert(os_deadlock_acquire_ordered(g, 1, 1));
    assert(os_deadlock_acquire_ordered(g, 1, 2));
    assert(os_deadlock_release_all(g, 1, &released) && released == 3);
    assert(os_deadlock_release_all(g, 1, &released) && released == 0);
    assert(!os_deadlock_acquire_ordered(g, 3, 0));
    assert(!os_deadlock_release_all(g, 3, &released));
    os_deadlock_destroy(g);
}

static void recovery_and_progress(void) {
    OsWaitGraph *g = NULL;
    assert(os_deadlock_create(3, 3, &g));
    for (size_t iteration = 0; iteration < 1000; ++iteration) {
        for (size_t i = 0; i < 3; ++i) {
            assert(os_deadlock_set_owner(g, i, i));
            assert(os_deadlock_set_wait(g, i, (i + 1) % 3));
        }
        size_t victim = 99, released = 99;
        assert(os_deadlock_recover_one(g, &victim, &released));
        assert(victim == 2 && released == 1);
        size_t cycle = 99;
        assert(os_deadlock_find_cycle(g, NULL, 0, &cycle) && cycle == 0);
        /* P1 can now obtain R2, finish, and release R1. P0 then obtains R1.
         * This bounded sequence demonstrates recovery and forward progress. */
        assert(os_deadlock_set_wait(g, 1, OS_DEADLOCK_NONE));
        assert(os_deadlock_acquire_ordered(g, 1, 2));
        assert(os_deadlock_release_all(g, 1, &released) && released == 2);
        assert(os_deadlock_set_wait(g, 0, OS_DEADLOCK_NONE));
        assert(os_deadlock_acquire_ordered(g, 0, 1));
        assert(os_deadlock_release_all(g, 0, &released) && released == 2);
        assert(os_deadlock_recover_one(g, &victim, &released));
        assert(victim == OS_DEADLOCK_NONE && released == 0);
    }
    os_deadlock_destroy(g);
}

int main(void) {
    assert(!os_deadlock_acquire_ordered(NULL, 0, 0));
    assert(!os_deadlock_release_all(NULL, 0, NULL));
    assert(!os_deadlock_recover_one(NULL, NULL, NULL));
    prevention();
    recovery_and_progress();
    return 0;
}
