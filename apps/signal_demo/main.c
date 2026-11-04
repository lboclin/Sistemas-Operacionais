#define _POSIX_C_SOURCE 200809L
#include "request.h"
#include "session.h"

#include <errno.h>
#include <inttypes.h>
#include <sys/select.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* The handler only records an intent; never snapshot or print from a signal. */
static volatile sig_atomic_t dump_requested = 0;

static void on_sigusr1(int signal_number) {
    (void)signal_number;
    dump_requested = 1;
}

static bool dump_if_requested(const OsVirtualSession *session) {
    if (!dump_requested) return true;
    dump_requested = 0;
    OsSessionSnapshot snapshot;
    OsRequest requests[2];
    OsWorkerSnapshot workers[1];
    if (!os_session_snapshot(session, &snapshot, requests, 2, workers, 1)) return false;
    printf("signal snapshot tick=%" PRIu64 " ready=%zu running=%zu blocked=%zu completed=%zu\n",
           snapshot.tick, snapshot.ready, snapshot.running,
           snapshot.blocked, snapshot.completed);
    printf("memory resident=%zu free=%zu faults=%" PRIu64 "\n",
           snapshot.resident_frames, snapshot.free_frames, snapshot.metrics.page_faults);
    for (size_t i = 0; i < 2; ++i)
        printf("request id=%" PRIu32 " state=%s tokens=%" PRIu32 "/%" PRIu32 "\n",
               requests[i].id, os_request_state_name(requests[i].state),
               requests[i].processed_tokens, requests[i].total_tokens);
    for (size_t i = 0; i < 1; ++i)
        if (workers[i].occupied)
            printf("worker %zu request=%" PRIu32 "\n", i, workers[i].request_id);
        else
            printf("worker %zu idle\n", i);
    return fflush(stdout) == 0;
}

int main(int argc, char **argv) {
    const bool automatic = argc == 2 && strcmp(argv[1], "--self-test") == 0;
    if (!automatic && argc != 1) {
        fprintf(stderr, "Usage: %s [--self-test]\n", argv[0]);
        return 2;
    }
    struct sigaction action = {0}, previous;
    action.sa_handler = on_sigusr1;
    if (sigemptyset(&action.sa_mask) != 0 ||
        sigaction(SIGUSR1, &action, &previous) != 0) {
        perror("sigaction");
        return 1;
    }
    int status = 0;
    OsRequest requests[2];
    OsVirtualSession *session = NULL;
    const OsVirtualConfig config = {
        .policy = OS_FCFS, .worker_count = 1, .memory_enabled = true,
        .frame_capacity = 2, .tokens_per_page = 2, .page_policy = OS_PAGE_FIFO,
        .page_fault_latency_ticks = 1
    };
    if (!os_request_init(&requests[0], 1, 0, 4, 0) ||
        !os_request_init(&requests[1], 2, 0, 3, 0) ||
        os_session_create(requests, 2, &config, NULL, NULL, &session) != OS_RUN_OK) {
        fputs("Unable to create signal demonstration session\n", stderr);
        status = 1;
        goto cleanup;
    }
    if (automatic) {
        if (os_session_step(session) != OS_RUN_OK || kill(getpid(), SIGUSR1) != 0 ||
            !dump_if_requested(session)) status = 1;
    } else {
        printf("PID=%ld. ENTER: advance simulation; kill -USR1 %ld: dump state; EOF: quit.\n",
               (long)getpid(), (long)getpid());
        fflush(stdout);
        sigset_t blocked_mask, old_mask;
        if (sigemptyset(&blocked_mask) != 0 ||
            sigaddset(&blocked_mask, SIGUSR1) != 0 ||
            sigprocmask(SIG_BLOCK, &blocked_mask, &old_mask) != 0) {
            status = 1;
            goto cleanup;
        }
        char input[8];
        for (;;) {
            if (!dump_if_requested(session)) { status = 1; break; }
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            /* pselect atomically unmasks SIGUSR1 while waiting: no lost wakeup
             * if the signal arrives between checking the flag and sleeping. */
            const int available = pselect(STDIN_FILENO + 1, &read_fds,
                                          NULL, NULL, NULL, &old_mask);
            if (available < 0) {
                if (errno == EINTR) continue;
                status = 1;
                break;
            }
            if (available > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
                if (fgets(input, sizeof(input), stdin) == NULL) {
                    if (ferror(stdin)) status = 1;
                    break;
                }
                if (os_session_step(session) != OS_RUN_OK) { status = 1; break; }
            }
        }
        if (sigprocmask(SIG_SETMASK, &old_mask, NULL) != 0) status = 1;
    }
cleanup:
    os_session_destroy(session);
    if (sigaction(SIGUSR1, &previous, NULL) != 0) status = 1;
    return status;
}
