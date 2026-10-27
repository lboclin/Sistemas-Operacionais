#include "pipe_channel.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void frames(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    assert(os_ipc_write_frame(fds[1], "abc", 3) == OS_IPC_OK);
    assert(os_ipc_write_frame(fds[1], NULL, 0) == OS_IPC_OK);
    assert(close(fds[1]) == 0);
    unsigned char out[8] = {0};
    size_t n = 99;
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_OK);
    assert(n == 3 && memcmp(out, "abc", 3) == 0);
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_OK && n == 0);
    n = 99;
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_EOF && n == 99);
    assert(close(fds[0]) == 0);
}
static void truncated_header(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    const unsigned char header[] = {0, 0};
    assert(write(fds[1], header, sizeof(header)) == (ssize_t)sizeof(header));
    assert(close(fds[1]) == 0);
    unsigned char out[4];
    size_t n = 99;
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_TRUNCATED);
    assert(n == 99);
    assert(close(fds[0]) == 0);
}
static void truncated_payload(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    const unsigned char frame[] = {0, 0, 0, 3, 'x'};
    assert(write(fds[1], frame, sizeof(frame)) == (ssize_t)sizeof(frame));
    assert(close(fds[1]) == 0);
    unsigned char out[4];
    size_t n = 99;
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_TRUNCATED);
    assert(n == 99);
    assert(close(fds[0]) == 0);
}
static void oversized(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    const unsigned char header[] = {0, 1, 0, 1}; /* 65537 */
    assert(write(fds[1], header, sizeof(header)) == (ssize_t)sizeof(header));
    assert(close(fds[1]) == 0);
    unsigned char out[4];
    size_t n = 99;
    assert(os_ipc_read_frame(fds[0], out, sizeof(out), &n) == OS_IPC_TOO_LARGE);
    assert(n == 99);
    assert(close(fds[0]) == 0);
    assert(pipe(fds) == 0);
    const unsigned char frame[] = {0, 0, 0, 3, 'a', 'b', 'c'};
    assert(write(fds[1], frame, sizeof(frame)) == (ssize_t)sizeof(frame));
    assert(close(fds[1]) == 0);
    assert(os_ipc_read_frame(fds[0], out, 2, &n) == OS_IPC_TOO_LARGE);
    assert(n == 99);
    assert(close(fds[0]) == 0);
}
int main(void) {
    size_t n = 7;
    assert(os_ipc_write_frame(-1, "a", 1) == OS_IPC_INVALID);
    assert(os_ipc_write_frame(1, NULL, 1) == OS_IPC_INVALID);
    assert(os_ipc_write_frame(1, "a", OS_IPC_MAX_PAYLOAD + 1u) == OS_IPC_INVALID);
    assert(os_ipc_read_frame(-1, NULL, 0, &n) == OS_IPC_INVALID);
    assert(os_ipc_read_frame(0, NULL, 1, &n) == OS_IPC_INVALID);
    assert(os_ipc_read_frame(0, NULL, 0, NULL) == OS_IPC_INVALID);
    frames(); truncated_header(); truncated_payload(); oversized();
    return 0;
}
