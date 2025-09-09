#include "core/decl.h"

#include <core/io.h>
#include <core/connection.h>
#include <core/buffer.h>

#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <unistd.h>

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count) {
    if (conn == NULL) {
        fprintf(stderr, "recv_buf: connection is NULL\n");
        exit(1);
    }

    if (buf == NULL) {
        fprintf(stderr, "recv_buf: buf is NULL\n");
        exit(1);
    }

    int fd = conn->fd; //NOLINT

    uint8_t *pos = buf;
    size_t space_left = count;
    ssize_t read = 0;
    
    for (;;) {
        if (space_left <= 0) {
            fprintf(stderr, "recv_buf: no space left to read into\n");
            exit(1);
        }

        ssize_t n = recv(fd, pos, space_left, 0);

        if (n == 0) {
            return 0;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == -1) {
            perror("recv_buf.recv");
            exit(1);
        }

        read += n;
        space_left -= n;
        pos += n * sizeof(*buf);
    }

    return read;
}

ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count) {
    if (conn == NULL) {
        fprintf(stderr, "send_buf: connection is NULL\n");
        exit(1);
    }

    if (buf == NULL) {
        fprintf(stderr, "send_buf: buf is NULL\n");
        exit(1);
    }

    int fd = conn->fd; //NOLINT

    uint8_t *pos = buf;
    ssize_t sent = 0;
    
    for (;;) {
        if (count == 0) {
            break;
        }

        ssize_t n = send(fd, pos, count, 0);

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == -1) {
            perror("send_buf.recv");
            exit(1);
        }

        sent += n;
        count -= n;
        pos += n * sizeof(*buf);
    }

    return sent;
}