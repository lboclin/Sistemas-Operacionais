#include "work_queue.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define PRODUCERS 4u
#define CONSUMERS 4u
#define PER_PRODUCER 1000u
#define TOTAL (PRODUCERS * PER_PRODUCER)

typedef struct {
    OsWorkQueue *queue;
    atomic_uint seen[TOTAL];
    atomic_uint processed;
} Stress;

typedef struct {
    Stress *stress;
    unsigned producer;
} ProducerArg;

static void *produce(void *context) {
    ProducerArg *arg = (ProducerArg *)context;
    for (unsigned i = 0; i < PER_PRODUCER; ++i) {
        const uint32_t id = arg->producer * PER_PRODUCER + i;
        assert(os_queue_push(arg->stress->queue,
                             (OsWorkItem){.request_id = id, .token_budget = 1}));
    }
    return NULL;
}

static void *consume(void *context) {
    Stress *stress = (Stress *)context;
    OsWorkItem item;
    while (os_queue_pop(stress->queue, &item)) {
        assert(item.request_id < TOTAL && item.token_budget == 1);
        (void)atomic_fetch_add_explicit(&stress->seen[item.request_id], 1,
                                         memory_order_relaxed);
        (void)atomic_fetch_add_explicit(&stress->processed, 1,
                                         memory_order_relaxed);
    }
    return NULL;
}

static void stress_test(void) {
    OsWorkQueue *queue = NULL;
    assert(os_queue_create(7, &queue));
    Stress stress = {.queue = queue};
    for (size_t i = 0; i < TOTAL; ++i) atomic_init(&stress.seen[i], 0);
    atomic_init(&stress.processed, 0);
    pthread_t producers[PRODUCERS], consumers[CONSUMERS];
    ProducerArg args[PRODUCERS];
    for (unsigned i = 0; i < CONSUMERS; ++i)
        assert(pthread_create(&consumers[i], NULL, consume, &stress) == 0);
    for (unsigned i = 0; i < PRODUCERS; ++i) {
        args[i] = (ProducerArg){.stress = &stress, .producer = i};
        assert(pthread_create(&producers[i], NULL, produce, &args[i]) == 0);
    }
    for (unsigned i = 0; i < PRODUCERS; ++i)
        assert(pthread_join(producers[i], NULL) == 0);
    os_queue_close(queue);
    os_queue_close(queue);
    for (unsigned i = 0; i < CONSUMERS; ++i)
        assert(pthread_join(consumers[i], NULL) == 0);
    assert(atomic_load_explicit(&stress.processed, memory_order_relaxed) == TOTAL);
    for (unsigned i = 0; i < TOTAL; ++i)
        assert(atomic_load_explicit(&stress.seen[i], memory_order_relaxed) == 1);
    assert(os_queue_size(queue) == 0 && os_queue_is_closed(queue));
    os_queue_destroy(queue);
}

typedef struct {
    OsWorkQueue *queue;
    bool pushed;
    bool popped;
    OsWorkItem item;
} Waiter;

static void *blocked_push(void *arg) {
    Waiter *w = (Waiter *)arg;
    w->pushed = os_queue_push(w->queue,
        (OsWorkItem){.request_id = 2, .token_budget = 1});
    return NULL;
}

static void *blocked_pop(void *arg) {
    Waiter *w = (Waiter *)arg;
    w->popped = os_queue_pop(w->queue, &w->item);
    return NULL;
}

static void close_tests(void) {
    OsWorkQueue *queue = NULL;
    assert(os_queue_create(1, &queue));
    assert(os_queue_push(queue, (OsWorkItem){.request_id = 1, .token_budget = 1}));
    assert(os_queue_size(queue) == 1);
    Waiter waiter = {.queue = queue};
    pthread_t thread;
    assert(pthread_create(&thread, NULL, blocked_push, &waiter) == 0);
    os_queue_close(queue);
    assert(pthread_join(thread, NULL) == 0);
    assert(!waiter.pushed && os_queue_is_closed(queue));
    OsWorkItem item = {0};
    assert(os_queue_pop(queue, &item) && item.request_id == 1);
    assert(!os_queue_pop(queue, &item));
    assert(!os_queue_push(queue, (OsWorkItem){.request_id = 3, .token_budget = 1}));
    os_queue_destroy(queue);

    assert(os_queue_create(1, &queue));
    waiter = (Waiter){.queue = queue};
    assert(pthread_create(&thread, NULL, blocked_pop, &waiter) == 0);
    os_queue_close(queue);
    assert(pthread_join(thread, NULL) == 0);
    assert(!waiter.popped);
    os_queue_destroy(queue);
}

int main(void) {
    OsWorkQueue *q = (OsWorkQueue *)1;
    assert(!os_queue_create(0, &q) && q == NULL);
    assert(!os_queue_create(65537, &q) && q == NULL);
    assert(!os_queue_create(1, NULL));
    assert(os_queue_size(NULL) == 0 && os_queue_is_closed(NULL));
    assert(!os_queue_push(NULL, (OsWorkItem){.token_budget = 1}));
    assert(!os_queue_pop(NULL, NULL));
    os_queue_close(NULL);
    os_queue_destroy(NULL);
    assert(os_queue_create(2, &q));
    assert(!os_queue_push(q, (OsWorkItem){.request_id = 1, .token_budget = 0}));
    assert(!os_queue_pop(q, NULL));
    os_queue_close(q);
    os_queue_destroy(q);
    close_tests();
    stress_test();
    return 0;
}
