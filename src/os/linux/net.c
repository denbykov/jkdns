#include "core/decl.h"

#include <core/net.h>
#include <core/connection.h>
#include <core/buffer.h>
#include <logger.h>

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count) {
    CHECK_INVARIANT(conn != NULL, "conn is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");

    int fd = conn->fd; // NOLINT

    uint8_t *pos = buf;
    size_t space_left = count;
    ssize_t read = 0;
    
    for (;;) {
        if (space_left <= 0) {
            // replace with errno
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
            return -1;
        }

        read += n;
        space_left -= n;
        pos += n * sizeof(*buf);
    }

    return read;
}

ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count) {
    CHECK_INVARIANT(conn != NULL, "conn is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");

    int fd = conn->fd; // NOLINT

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
            return -1;
        }

        sent += n;
        count -= n;
        pos += n * sizeof(*buf);
    }

    return sent;
}

int64_t open_tcp_conn(const char* ip, uint16_t port) {
    int64_t fd = 0;
    fd = socket(AF_INET,SOCK_STREAM,0);
    CHECK_INVARIANT(fd != -1, "failed to create socket");

    int flags = fcntl(fd, F_GETFL, 0); // NOLINT
    if (flags == -1) {
        perror("open_tcp_conn.fcntl_get_flags");
        close(fd); // NOLINT
        return -1;
    };
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) { // NOLINT
        perror("open_tcp_conn.fcntl_set_non_blocking");
        close(fd); // NOLINT
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("open_tcp_conn.inet_pton");
        close(fd); // NOLINT
        return -1;
    }

    int ret = connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); // NOLINT
    if (ret < 0 && errno != EINPROGRESS) {
        perror("open_tcp_conn.connect");
        close(fd); // NOLINT
        return -1;
    }

    return fd;
}

void close_tcp_conn(int64_t fd) {
    close(fd); // NOLINT
}
