#include "deadlock.h"

#include <stdint.h>
#include <stdlib.h>

#define OS_GRAPH_MAX 128u

struct OsWaitGraph {
    size_t processes;
    size_t resources;
    size_t *owners;
    size_t *waiting;
};

bool os_deadlock_create(size_t process_count, size_t resource_count,
                        OsWaitGraph **out) {
    if (out == NULL) return false;
    *out = NULL;
    if (process_count == 0 || process_count > OS_GRAPH_MAX ||
        resource_count == 0 || resource_count > OS_GRAPH_MAX) return false;
    OsWaitGraph *graph = calloc(1, sizeof(*graph));
    if (graph == NULL) return false;
    graph->owners = malloc(resource_count * sizeof(*graph->owners));
    graph->waiting = malloc(process_count * sizeof(*graph->waiting));
    if (graph->owners == NULL || graph->waiting == NULL) {
        os_deadlock_destroy(graph);
        return false;
    }
    graph->processes = process_count;
    graph->resources = resource_count;
    for (size_t i = 0; i < resource_count; ++i) graph->owners[i] = OS_DEADLOCK_NONE;
    for (size_t i = 0; i < process_count; ++i) graph->waiting[i] = OS_DEADLOCK_NONE;
    *out = graph;
    return true;
}

bool os_deadlock_set_owner(OsWaitGraph *graph, size_t resource, size_t process) {
    if (graph == NULL || resource >= graph->resources ||
        (process != OS_DEADLOCK_NONE && process >= graph->processes)) return false;
    graph->owners[resource] = process;
    return true;
}

bool os_deadlock_set_wait(OsWaitGraph *graph, size_t process, size_t resource) {
    if (graph == NULL || process >= graph->processes ||
        (resource != OS_DEADLOCK_NONE && resource >= graph->resources)) return false;
    graph->waiting[process] = resource;
    return true;
}

bool os_deadlock_find_cycle(const OsWaitGraph *graph, size_t *cycle,
                            size_t capacity, size_t *count) {
    if (graph == NULL || count == NULL || (cycle == NULL && capacity != 0))
        return false;
    size_t seen[OS_GRAPH_MAX];
    size_t path[OS_GRAPH_MAX];
    for (size_t start = 0; start < graph->processes; ++start) {
        for (size_t i = 0; i < graph->processes; ++i) seen[i] = OS_DEADLOCK_NONE;
        size_t length = 0;
        size_t current = start;
        while (current != OS_DEADLOCK_NONE && seen[current] == OS_DEADLOCK_NONE) {
            seen[current] = length;
            path[length++] = current;
            const size_t waiting = graph->waiting[current];
            current = waiting == OS_DEADLOCK_NONE ? OS_DEADLOCK_NONE : graph->owners[waiting];
        }
        if (current == OS_DEADLOCK_NONE) continue;
        const size_t begin = seen[current];
        const size_t needed = length - begin;
        if (cycle != NULL && capacity < needed) return false;
        if (cycle != NULL)
            for (size_t i = 0; i < needed; ++i) cycle[i] = path[begin + i];
        *count = needed;
        return true;
    }
    *count = 0;
    return true;
}

void os_deadlock_destroy(OsWaitGraph *graph) {
    if (graph == NULL) return;
    free(graph->owners);
    free(graph->waiting);
    free(graph);
}
