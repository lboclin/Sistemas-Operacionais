#define _POSIX_C_SOURCE 200809L
#include "parallel_runtime.h"
#include "pipe_channel.h"
#include "request.h"
#include "virtual.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Wire format v1: big-endian integers, no native structs or pointers.
 * Request: "OLI1", count, workers, policy(0=FCFS,1=RR), quantum,
 *          then count * (id:u32, arrival:u64, total:u32, predicted:u32, priority:u32).
 * Reply:   "OLR1", status, count, completed, makespan:u64,
 *          then count * (id:u32, state:u32, generated:u32, finished:u64).
 * This demo deliberately supports at most 8 nonnegative-priority PCBs. */
#define MAX_REQUESTS 8u
#define REQUEST_HEADER 20u
#define REQUEST_RECORD 24u
#define REPLY_HEADER 24u
#define REPLY_RECORD 20u
#define BUFFER_SIZE 256u

static void put32(unsigned char *buffer, size_t *at, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        buffer[(*at)++] = (unsigned char)(value >> shift);
}
static void put64(unsigned char *buffer, size_t *at, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        buffer[(*at)++] = (unsigned char)(value >> shift);
}
static uint32_t get32(const unsigned char *buffer, size_t *at) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value = (value << 8) | buffer[(*at)++];
    return value;
}
static uint64_t get64(const unsigned char *buffer, size_t *at) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value = (value << 8) | buffer[(*at)++];
    return value;
}
static bool create_requests(OsRequest *requests) {
    return os_request_init(&requests[0], 1, 0, 4, 3, 1) &&
           os_request_init(&requests[1], 2, 1, 3, 4, 0) &&
           os_request_init(&requests[2], 3, 2, 2, 2, 2);
}
static size_t encode_request(unsigned char *buffer, const OsRequest *requests,
                             size_t count, bool round_robin) {
    size_t at = 0;
    memcpy(buffer, "OLI1", 4);
    at = 4;
    put32(buffer, &at, (uint32_t)count);
    put32(buffer, &at, 2);
    put32(buffer, &at, round_robin ? 1u : 0u);
    put32(buffer, &at, 2);
    for (size_t i = 0; i < count; ++i) {
        put32(buffer, &at, requests[i].id);
        put64(buffer, &at, requests[i].arrival_tick);
        put32(buffer, &at, requests[i].total_tokens);
        put32(buffer, &at, requests[i].predicted_tokens);
        put32(buffer, &at, (uint32_t)requests[i].priority);
    }
    return at;
}
static bool decode_request(const unsigned char *buffer, size_t length,
                           OsRequest *requests, size_t *count,
                           OsVirtualConfig *config) {
    if (length < REQUEST_HEADER || memcmp(buffer, "OLI1", 4) != 0) return false;
    size_t at = 4;
    const uint32_t n = get32(buffer, &at);
    const uint32_t workers = get32(buffer, &at);
    const uint32_t policy = get32(buffer, &at);
    const uint32_t quantum = get32(buffer, &at);
    if (n == 0 || n > MAX_REQUESTS ||
        length != REQUEST_HEADER + (size_t)n * REQUEST_RECORD ||
        workers == 0 || workers > n || policy > 1 || quantum == 0) return false;
    *config = (OsVirtualConfig){.policy = policy ? OS_ROUND_ROBIN : OS_FCFS,
                                .worker_count = workers, .quantum_tokens = quantum};
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t id = get32(buffer, &at);
        const uint64_t arrival = get64(buffer, &at);
        const uint32_t total = get32(buffer, &at);
        const uint32_t predicted = get32(buffer, &at);
        const uint32_t priority = get32(buffer, &at);
        if (priority > INT32_MAX ||
            !os_request_init(&requests[i], id, arrival, total, predicted, (int)priority))
            return false;
        for (uint32_t j = 0; j < i; ++j)
            if (requests[j].id == id) return false;
    }
    *count = n;
    return true;
}
static size_t encode_reply(unsigned char *buffer, const OsRequest *requests,
                           size_t count, const OsRunSummary *summary) {
    size_t at = 4;
    memcpy(buffer, "OLR1", 4);
    put32(buffer, &at, (uint32_t)OS_RUN_OK);
    put32(buffer, &at, (uint32_t)count);
    put32(buffer, &at, (uint32_t)summary->completed);
    put64(buffer, &at, summary->makespan_ticks);
    for (size_t i = 0; i < count; ++i) {
        put32(buffer, &at, requests[i].id);
        put32(buffer, &at, (uint32_t)requests[i].state);
        put32(buffer, &at, requests[i].generated_tokens);
        put64(buffer, &at, requests[i].finished_tick);
    }
    return at;
}
static bool verify_reply(const unsigned char *buffer, size_t length,
                         const OsRequest *expected, size_t count,
                         const OsRunSummary *summary) {
    if (length != REPLY_HEADER + count * REPLY_RECORD ||
        memcmp(buffer, "OLR1", 4) != 0) return false;
    size_t at = 4;
    if (get32(buffer, &at) != OS_RUN_OK || get32(buffer, &at) != count ||
        get32(buffer, &at) != summary->completed ||
        get64(buffer, &at) != summary->makespan_ticks) return false;
    for (size_t i = 0; i < count; ++i) {
        if (get32(buffer, &at) != expected[i].id ||
            get32(buffer, &at) != (uint32_t)expected[i].state ||
            get32(buffer, &at) != expected[i].generated_tokens ||
            get64(buffer, &at) != expected[i].finished_tick) return false;
    }
    return true;
}
static int child_run(int input, int output) {
    unsigned char buffer[BUFFER_SIZE];
    size_t length = 0;
    if (os_ipc_read_frame(input, buffer, sizeof(buffer), &length) != OS_IPC_OK)
        return 2;
    OsRequest requests[MAX_REQUESTS];
    OsVirtualConfig config = {0};
    size_t count = 0;
    if (!decode_request(buffer, length, requests, &count, &config)) return 2;
    OsRunSummary summary = {0};
    if (os_posix_run(requests, count, &config, &summary, NULL, NULL) != OS_RUN_OK)
        return 3;
    length = encode_reply(buffer, requests, count, &summary);
    return os_ipc_write_frame(output, buffer, length) == OS_IPC_OK ? 0 : 4;
}
int main(int argc, char **argv) {
    const bool malformed = argc == 2 && strcmp(argv[1], "--malformed") == 0;
    const bool rr = argc == 1 || (argc == 2 && strcmp(argv[1], "--rr") == 0);
    const bool fcfs = argc == 2 && strcmp(argv[1], "--fcfs") == 0;
    if (!rr && !fcfs && !malformed) {
        fprintf(stderr, "Usage: %s [--rr|--fcfs|--malformed]\n", argv[0]);
        return 2;
    }
    OsRequest requests[3];
    if (!create_requests(requests)) return 1;
    const OsVirtualConfig config = {.policy = fcfs ? OS_FCFS : OS_ROUND_ROBIN,
                                    .worker_count = 2, .quantum_tokens = 2};
    OsRunSummary baseline = {0};
    if (!malformed && os_virtual_run(requests, 3, &config, &baseline) != OS_RUN_OK)
        return 1;
    int to_child[2], from_child[2];
    if (pipe(to_child) != 0) return 1;
    if (pipe(from_child) != 0) {
        (void)close(to_child[0]); (void)close(to_child[1]);
        return 1;
    }
    const pid_t child = fork();
    if (child < 0) {
        (void)close(to_child[0]); (void)close(to_child[1]);
        (void)close(from_child[0]); (void)close(from_child[1]);
        return 1;
    }
    if (child == 0) {
        (void)close(to_child[1]); (void)close(from_child[0]);
        const int result = child_run(to_child[0], from_child[1]);
        (void)close(to_child[0]); (void)close(from_child[1]);
        _exit(result);
    }
    (void)close(to_child[0]); (void)close(from_child[1]);
    unsigned char request[BUFFER_SIZE], reply[BUFFER_SIZE];
    size_t length = encode_request(request, requests, 3, !fcfs);
    if (malformed) request[0] = 'X';
    const OsIpcStatus sent = os_ipc_write_frame(to_child[1], request, length);
    (void)close(to_child[1]);
    size_t received_length = 0;
    const OsIpcStatus received = os_ipc_read_frame(from_child[0], reply,
                                                    sizeof(reply), &received_length);
    (void)close(from_child[0]);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (sent != OS_IPC_OK || waited != child || !WIFEXITED(status)) return 1;
    if (malformed) {
        if (WEXITSTATUS(status) != 2 || received != OS_IPC_EOF) return 1;
        puts("ipc runtime rejected malformed request");
        return 0;
    }
    if (WEXITSTATUS(status) != 0 || received != OS_IPC_OK ||
        !verify_reply(reply, received_length, requests, 3, &baseline)) return 1;
    printf("ipc runtime policy=%s completed=%zu makespan=%llu verified=yes\n",
           fcfs ? "fcfs" : "rr", baseline.completed,
           (unsigned long long)baseline.makespan_ticks);
    return 0;
}
