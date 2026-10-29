#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* A teaching lab only. Deliberate deadlock is confined to a child process;
 * the parent never owns either pthread mutex and can always reap the child. */
typedef struct {
    pthread_mutex_t resources[2];
    pthread_barrier_t both_holding_first;
    bool circular;
    unsigned completed;
} Lab;

typedef struct {
    Lab *lab;
    unsigned index;
} WorkerArg;

static char worker_failure;

static void *worker_main(void *data) {
    const WorkerArg *arg = data;
    Lab *lab = arg->lab;
    const unsigned first = lab->circular ? arg->index : 0u;
    const unsigned second = lab->circular ? 1u - first : 1u;
    if (pthread_mutex_lock(&lab->resources[first]) != 0) return &worker_failure;
    if (lab->circular) {
        const int rc = pthread_barrier_wait(&lab->both_holding_first);
        if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) return &worker_failure;
    }
    /* Ordered: both take 0 then 1, so they eventually finish.
     * Circular: each holds a different first mutex, then waits for the other. */
    if (pthread_mutex_lock(&lab->resources[second]) != 0) return &worker_failure;
    ++lab->completed; /* In ordered mode, mutex 0 serializes this write. */
    if (pthread_mutex_unlock(&lab->resources[second]) != 0 ||
        pthread_mutex_unlock(&lab->resources[first]) != 0) return &worker_failure;
    return NULL;
}

static bool send_marker(int fd, char marker) {
    for (;;) {
        const ssize_t n = write(fd, &marker, 1);
        if (n == 1) return true;
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
}

static int child_run(bool circular, int notify_fd) {
    Lab lab = {.circular = circular};
    if (pthread_mutex_init(&lab.resources[0], NULL) != 0) return 2;
    if (pthread_mutex_init(&lab.resources[1], NULL) != 0) return 2;
    if (circular && pthread_barrier_init(&lab.both_holding_first, NULL, 3) != 0)
        return 2;
    WorkerArg args[2] = {{.lab = &lab, .index = 0}, {.lab = &lab, .index = 1}};
    pthread_t workers[2];
    if (pthread_create(&workers[0], NULL, worker_main, &args[0]) != 0 ||
        pthread_create(&workers[1], NULL, worker_main, &args[1]) != 0)
        return 2; /* _exit in the caller terminates any started threads. */
    if (circular) {
        /* Both threads have acquired different first mutexes. The barrier
         * releases them to attempt the opposite lock before reporting ready. */
        const int rc = pthread_barrier_wait(&lab.both_holding_first);
        if ((rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) ||
            !send_marker(notify_fd, 'R')) return 2;
    }
    void *result[2] = {NULL, NULL};
    if (pthread_join(workers[0], &result[0]) != 0 ||
        pthread_join(workers[1], &result[1]) != 0 ||
        result[0] != NULL || result[1] != NULL || lab.completed != 2)
        return 2;
    if (!send_marker(notify_fd, 'O')) return 2;
    if (circular && pthread_barrier_destroy(&lab.both_holding_first) != 0) return 2;
    if (pthread_mutex_destroy(&lab.resources[1]) != 0 ||
        pthread_mutex_destroy(&lab.resources[0]) != 0) return 2;
    return 0;
}

static bool reap(pid_t child, int *status) {
    for (;;) {
        const pid_t result = waitpid(child, status, 0);
        if (result == child) return true;
        if (result < 0 && errno == EINTR) continue;
        return false;
    }
}

static bool kill_and_reap(pid_t child, int *status) {
    if (kill(child, SIGKILL) != 0 && errno != ESRCH) return false;
    return reap(child, status);
}

static bool expired(const struct timespec *start, int seconds) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return true;
    const time_t elapsed = now.tv_sec - start->tv_sec;
    return elapsed > seconds ||
           (elapsed == seconds && now.tv_nsec >= start->tv_nsec);
}

static int parent_run(bool circular) {
    int channel[2];
    if (pipe(channel) != 0) return 2;
    const pid_t child = fork();
    if (child < 0) {
        (void)close(channel[0]);
        (void)close(channel[1]);
        return 2;
    }
    if (child == 0) {
        (void)close(channel[0]);
        const int result = child_run(circular, channel[1]);
        (void)close(channel[1]);
        _exit(result);
    }
    (void)close(channel[1]);
    const int flags = fcntl(channel[0], F_GETFL);
    struct timespec start;
    if (flags < 0 || fcntl(channel[0], F_SETFL, flags | O_NONBLOCK) != 0 ||
        clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        int status = 0;
        (void)kill_and_reap(child, &status);
        (void)close(channel[0]);
        return 2;
    }
    bool marker_seen = false;
    bool bad_marker = false;
    const char expected = circular ? 'R' : 'O';
    int status = 0;
    for (;;) {
        char marker;
        const ssize_t n = read(channel[0], &marker, 1);
        if (n == 1) {
            if (marker_seen || marker != expected) bad_marker = true;
            marker_seen = true;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            bad_marker = true;
        }
        const pid_t finished = waitpid(child, &status, WNOHANG);
        if (finished == child) {
            /* Drain a marker that may have been written just before exit. */
            if (!marker_seen) {
                const ssize_t last = read(channel[0], &marker, 1);
                if (last == 1) {
                    marker_seen = true;
                    if (marker != expected) bad_marker = true;
                }
            }
            (void)close(channel[0]);
            if (!circular && !bad_marker && marker_seen &&
                WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                puts("mode=ordered completed=2 watchdog=none deadlock=no");
                return 0;
            }
            fputs("Child exited without the expected result\n", stderr);
            return 1;
        }
        if (finished < 0 || bad_marker) {
            (void)kill_and_reap(child, &status);
            (void)close(channel[0]);
            fputs("Child supervision failed\n", stderr);
            return 1;
        }
        if (expired(&start, circular ? 2 : 5)) {
            const bool killed = kill_and_reap(child, &status);
            (void)close(channel[0]);
            if (circular && marker_seen && killed &&
                WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
                puts("mode=deadlock acquired=2 watchdog=SIGKILL deadlock=observed");
                return 0;
            }
            fputs("Watchdog timeout without verified circular wait\n", stderr);
            return 1;
        }
        const struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000L};
        (void)nanosleep(&pause, NULL);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ordered|deadlock\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "ordered") == 0) return parent_run(false);
    if (strcmp(argv[1], "deadlock") == 0) return parent_run(true);
    fprintf(stderr, "Usage: %s ordered|deadlock\n", argv[0]);
    return 2;
}
