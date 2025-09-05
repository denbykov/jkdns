#include "core/decl.h"

#include <core/io.h>
#include <core/connection.h>
#include <core/buffer.h>

#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>

ssize_t recv_buf(connection_t *conn, buffer_t* buf) {
    if (conn == NULL) {
        fprintf(stderr, "recv_buf: connection is NULL\n"); //NOLINT
        exit(1);
    }

    if (buf == NULL) {
        fprintf(stderr, "recv_buf: buffer is NULL\n"); //NOLINT
        exit(1);
    }

    if (buf->data == NULL) {
        fprintf(stderr, "recv_buf: buffer has no space allocated\n"); //NOLINT
        exit(1);
    }

    int fd = conn->fd;

    uint8_t *pos = buf->data + buf->taken * sizeof(*buf->data);
    size_t space_left = buf->capacity - buf->taken;
    ssize_t read = 0;
    
    for (;;) {
        if (space_left <= 0) {
            fprintf(stderr, "recv_buf: no space left to read into\n"); //NOLINT
            exit(1);
        }

        ssize_t n = recv(fd, pos, space_left, 0);

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == -1) {
            perror("recv_buf.recv");
            exit(1);
        }

        read += n;
        buf->taken += n;
        space_left -= n;
        pos += n * sizeof(*buf->data);
    }

    return read;
}

ssize_t send_buf(connection_t *conn, buffer_t* buf) {
    (void)conn;
    (void)buf;

    ssize_t sent = 0;

    return sent;
}