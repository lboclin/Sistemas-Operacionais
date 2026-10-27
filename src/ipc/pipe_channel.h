#ifndef OS_PIPE_CHANNEL_H
#define OS_PIPE_CHANNEL_H
#include <stddef.h>
/* POSIX pipe protocol: 4-byte big-endian length + <=65536 payload bytes.
 * Caller owns descriptors, lifecycle and SIGPIPE policy. */
#define OS_IPC_MAX_PAYLOAD 65536u
typedef enum {
    OS_IPC_OK, OS_IPC_EOF, OS_IPC_TRUNCATED,
    OS_IPC_TOO_LARGE, OS_IPC_IO_ERROR, OS_IPC_INVALID
} OsIpcStatus;
/* A zero-length frame may use payload=NULL. */
OsIpcStatus os_ipc_write_frame(int fd, const void *payload, size_t length);
/* On success only, sets out_length. After a framing error, close the channel. */
OsIpcStatus os_ipc_read_frame(int fd, void *buffer, size_t capacity,
                              size_t *out_length);
#endif
