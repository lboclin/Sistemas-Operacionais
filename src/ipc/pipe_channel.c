#include "pipe_channel.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

static OsIpcStatus write_exact(int fd, const unsigned char *data, size_t size) {
    while (size != 0) {
        const ssize_t n = write(fd, data, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return OS_IPC_IO_ERROR;
        data += (size_t)n;
        size -= (size_t)n;
    }
    return OS_IPC_OK;
}
static OsIpcStatus read_exact(int fd, unsigned char *data, size_t size, bool header) {
    size_t consumed = 0;
    while (consumed < size) {
        const ssize_t n = read(fd, data + consumed, size - consumed);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return OS_IPC_IO_ERROR;
        if (n == 0) return header && consumed == 0 ? OS_IPC_EOF : OS_IPC_TRUNCATED;
        consumed += (size_t)n;
    }
    return OS_IPC_OK;
}
OsIpcStatus os_ipc_write_frame(int fd, const void *payload, size_t length) {
    if (fd < 0 || length > OS_IPC_MAX_PAYLOAD || (length && payload == NULL))
        return OS_IPC_INVALID;
    const uint32_t n = (uint32_t)length;
    const unsigned char header[4] = {
        (unsigned char)(n >> 24), (unsigned char)(n >> 16),
        (unsigned char)(n >> 8), (unsigned char)n
    };
    const OsIpcStatus status = write_exact(fd, header, sizeof(header));
    if (status != OS_IPC_OK) return status;
    return write_exact(fd, (const unsigned char *)payload, length);
}
OsIpcStatus os_ipc_read_frame(int fd, void *buffer, size_t capacity,
                              size_t *out_length) {
    if (fd < 0 || out_length == NULL || (capacity && buffer == NULL))
        return OS_IPC_INVALID;
    unsigned char header[4];
    const OsIpcStatus status = read_exact(fd, header, sizeof(header), true);
    if (status != OS_IPC_OK) return status;
    const uint32_t n = ((uint32_t)header[0] << 24) |
                       ((uint32_t)header[1] << 16) |
                       ((uint32_t)header[2] << 8) | (uint32_t)header[3];
    if (n > OS_IPC_MAX_PAYLOAD || (size_t)n > capacity) return OS_IPC_TOO_LARGE;
    const OsIpcStatus payload_status = read_exact(fd, (unsigned char *)buffer,
                                                   (size_t)n, false);
    if (payload_status != OS_IPC_OK) return payload_status;
    *out_length = (size_t)n;
    return OS_IPC_OK;
}
