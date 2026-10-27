#include "pipe_channel.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int worker(int input, int output) {
    unsigned char buffer[64];
    size_t size = 0;
    if (os_ipc_read_frame(input, buffer, sizeof(buffer), &size) != OS_IPC_OK)
        return 1;
    for (size_t i = 0; i < size; ++i) buffer[i] = (unsigned char)toupper(buffer[i]);
    return os_ipc_write_frame(output, buffer, size) == OS_IPC_OK ? 0 : 1;
}
int main(void) {
    int requests[2];
    int responses[2];
    if (pipe(requests) != 0) return 1;
    if (pipe(responses) != 0) {
        (void)close(requests[0]); (void)close(requests[1]);
        return 1;
    }
    const pid_t child = fork();
    if (child < 0) {
        (void)close(requests[0]); (void)close(requests[1]);
        (void)close(responses[0]); (void)close(responses[1]);
        return 1;
    }
    if (child == 0) {
        (void)close(requests[1]); (void)close(responses[0]);
        const int result = worker(requests[0], responses[1]);
        (void)close(requests[0]); (void)close(responses[1]);
        _exit(result);
    }
    (void)close(requests[0]); (void)close(responses[1]);
    static const char request[] = "hello ipc";
    unsigned char reply[64] = {0};
    size_t reply_size = 0;
    const OsIpcStatus sent = os_ipc_write_frame(requests[1], request, sizeof(request) - 1);
    (void)close(requests[1]);
    const OsIpcStatus received = os_ipc_read_frame(responses[0], reply,
                                                     sizeof(reply) - 1, &reply_size);
    (void)close(responses[0]);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (sent != OS_IPC_OK || received != OS_IPC_OK || waited != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        reply_size != sizeof(request) - 1 ||
        memcmp(reply, "HELLO IPC", reply_size) != 0) return 1;
    reply[reply_size] = '\0';
    printf("gateway sent=%s worker replied=%s\n", request, reply);
    return 0;
}
