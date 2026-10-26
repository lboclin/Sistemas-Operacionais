#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    atomic_uint counter;
    pthread_barrier_t before_write;
    bool safe;
} RaceState;

static void *increment(void *opaque) {
    RaceState *state = (RaceState *)opaque;
    /* Both workers read the same value before either writes. This forces a
     * lost update without a C data race (atomic load/store are well-defined). */
    const unsigned snapshot = atomic_load_explicit(&state->counter, memory_order_relaxed);
    (void)pthread_barrier_wait(&state->before_write);
    if (state->safe) {
        (void)atomic_fetch_add_explicit(&state->counter, 1, memory_order_relaxed);
    } else {
        atomic_store_explicit(&state->counter, snapshot + 1, memory_order_relaxed);
    }
    return NULL;
}

static int child_run(bool safe) {
    RaceState state = {.safe = safe};
    atomic_init(&state.counter, 0);
    if (pthread_barrier_init(&state.before_write, NULL, 2) != 0) return 2;
    pthread_t first, second;
    if (pthread_create(&first, NULL, increment, &state) != 0) {
        (void)pthread_barrier_destroy(&state.before_write);
        return 2;
    }
    if (pthread_create(&second, NULL, increment, &state) != 0) {
        /* The first thread is waiting on the barrier. Exit the isolated
         * process instead of attempting an unbounded join. */
        return 2;
    }
    if (pthread_join(first, NULL) != 0 || pthread_join(second, NULL) != 0) return 2;
    (void)pthread_barrier_destroy(&state.before_write);
    const unsigned observed = atomic_load_explicit(&state.counter, memory_order_relaxed);
    printf("mode=%s expected=2 observed=%u lost=%u\n", safe ? "safe" : "unsafe",
           observed, 2u - observed);
    return observed == (safe ? 2u : 1u) ? 0 : 1;
}

static int supervise(const char *program, const char *mode) {
    const pid_t pid = fork();
    if (pid < 0) return 2;
    if (pid == 0) {
        char *const args[] = {(char *)program, "--child", (char *)mode, NULL};
        execv(program, args);
        _exit(127);
    }
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        (void)kill(pid, SIGKILL);
        (void)waitpid(pid, NULL, 0);
        return 2;
    }
    const time_t deadline = now.tv_sec + 3;
    int status = 0;
    for (;;) {
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) break;
        if (result < 0) {
            if (errno == EINTR) continue;
            return 2;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec >= deadline) {
            (void)kill(pid, SIGKILL);
            (void)waitpid(pid, NULL, 0);
            fputs("race laboratory timeout; child killed\n", stderr);
            return 124;
        }
        const struct timespec nap = {.tv_sec = 0, .tv_nsec = 10000000L};
        (void)nanosleep(&nap, NULL);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}

int main(int argc, char **argv) {
    if ((argc != 2 && argc != 3) ||
        (argc == 3 && strcmp(argv[1], "--child") != 0)) {
        fputs("Usage: os_llm_race_lab safe|unsafe\n", stderr);
        return 2;
    }
    const char *mode = argv[argc - 1];
    if (strcmp(mode, "safe") != 0 && strcmp(mode, "unsafe") != 0) {
        fputs("mode must be safe or unsafe\n", stderr);
        return 2;
    }
    return argc == 3 ? child_run(strcmp(mode, "safe") == 0) : supervise(argv[0], mode);
}
