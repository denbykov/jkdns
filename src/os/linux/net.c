#include "core/decl.h"
#include "core/errors.h"

#include "core/net.h"
#include "core/connection.h"
#include "core/buffer.h"
#include "core/udp_socket.h"
#include "logger/logger.h"

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

static ssize_t tcp_recv_buf(connection_t *conn, uint8_t* buf, size_t count);
static ssize_t udp_recv_buf(connection_t *conn, uint8_t* buf, size_t count);
static ssize_t tcp_send_buf(connection_t *conn, uint8_t* buf, size_t count);
static ssize_t udp_send_buf(connection_t *conn, uint8_t* buf, size_t count);

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn != NULL, "conn is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");

    if (conn->handle.type == CONN_TYPE_TCP) {
        return tcp_recv_buf(conn, buf, count);
    } else if (conn->handle.type == CONN_TYPE_UDP) {
        return udp_recv_buf(conn, buf, count);
    } else {
        PANIC("Unexpected connection type");
    }

    return JK_ERROR;
}

static ssize_t tcp_recv_buf(connection_t *conn, uint8_t* buf, size_t count) {
    logger_t* logger = current_logger;

    int fd = conn->handle.data.fd; // NOLINT

    uint8_t *pos = buf;
    size_t space_left = count;
    ssize_t read = 0;
    
    for (;;) {
        if (space_left <= 0) {
            log_warn("tcp_recv_buf: no space left to read into");
            return JK_OUT_OF_BUFFER;
        }

        ssize_t n = recv(fd, pos, space_left, 0);

        if (n == 0) {
            return 0;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == -1) {
            return JK_ERROR;
        }

        read += n;
        space_left -= n;
        pos += n * sizeof(*buf);
    }

    return read;
}

static ssize_t udp_recv_buf(connection_t *conn, uint8_t* buf, size_t count) {
    logger_t* logger = current_logger;

    udp_socket_t *sock = conn->handle.data.sock;

    CHECK_INVARIANT(sock != NULL, "sock is NULL");
    CHECK_INVARIANT(sock->last_read_buf.taken <= count, "cannot copy whole buffer");

    memcpy(buf, sock->last_read_buf.data, sock->last_read_buf.taken);

    return (ssize_t)sock->last_read_buf.taken;
}

ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn != NULL, "conn is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");

    if (conn->handle.type == CONN_TYPE_TCP) {
        return tcp_send_buf(conn, buf, count);
    } else if (conn->handle.type == CONN_TYPE_UDP) {
        return udp_send_buf(conn, buf, count);
    } else {
        PANIC("Unexpected connection type");
    }

    return JK_ERROR;
}

ssize_t tcp_send_buf(connection_t *conn, uint8_t* buf, size_t count) {
    int fd = conn->handle.data.fd; // NOLINT

    uint8_t *pos = buf;
    ssize_t sent = 0;
    
    for (;;) {
        if (count == 0) {
            break;
        }

        ssize_t n = send(fd, pos, count, 0);

        if (n == 0) {
            return 0;
        }

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == -1) {
            return JK_ERROR;
        }

        sent += n;
        count -= n;
        pos += n * sizeof(*buf);
    }

    return sent;
}

static ssize_t udp_send_buf(connection_t *conn, uint8_t* buf, size_t count) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn != NULL, "conn is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");
    CHECK_INVARIANT(count != 0, "count is 0");

    udp_socket_t *sock = conn->handle.data.sock;
    
    CHECK_INVARIANT(sock != NULL, "sock is null");
    
    int fd = (int)sock->fd;
    ssize_t sent = 0;

    if (conn->address.af == AF_INET) {
        struct sockaddr_in sa4 = {0};
        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(conn->address.src_port);
        sa4.sin_addr = conn->address.src.src_v4;

        sent = 
            sendto(fd, buf, count, 0,
            (struct sockaddr*)&sa4, sizeof(sa4));

    } else if (conn->address.af == AF_INET6) {
        struct sockaddr_in6 sa6 = {0};
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(conn->address.src_port);
        sa6.sin6_addr = conn->address.src.src_v6;

        sent = 
            sendto(fd, buf, count, 0,
             (struct sockaddr*)&sa6, sizeof(sa6));
    } else {
        PANIC("Unrecognized address family");
    }

    if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        sock->writable = false;
        return JK_WOULD_BLOCK;
    }

    if (sent == -1) {
        log_perror("udp_send_buf.sendto");
        return JK_ERROR;
    }

    return sent;
}

int64_t open_tcp_conn(const char* ip, uint16_t port) {
    logger_t* logger = current_logger;

    int fd = 0;
    fd = socket(AF_INET,SOCK_STREAM,0);
    CHECK_INVARIANT(fd != -1, "failed to create socket");

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        log_perror("open_tcp_conn.fcntl_get_flags");
        close(fd);
        return JK_ERROR;
    };
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        log_perror("open_tcp_conn.fcntl_set_non_blocking");
        close(fd);
        return JK_ERROR;
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        log_perror("open_tcp_conn.inet_pton");
        close(fd);
        return JK_ERROR;
    }
    
    int ret = connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (ret < 0 && errno != EINPROGRESS) {
        log_perror("open_tcp_conn.connect");
        close(fd);
        return JK_ERROR;
    }

    return fd;
}

void close_tcp_conn(int64_t fd) {
    close(fd); // NOLINT
}

ssize_t udp_recv(udp_socket_t *sock, uint8_t* buf, size_t count, address_t* address) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(sock != NULL, "sock is null");
    CHECK_INVARIANT(buf != NULL, "buf is null");
    CHECK_INVARIANT(address != NULL, "address is null");
    CHECK_INVARIANT(count != 0, "count is 0");

    int fd = sock->fd; // NOLINT

    struct sockaddr_storage peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    socklen_t peer_addr_len = sizeof(peer_addr);

    ssize_t n = recvfrom(
        fd, buf, count, 0,
		(struct sockaddr *)&peer_addr, &peer_addr_len);

    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return JK_WOULD_BLOCK;
    }

    if (n == -1) {
        log_perror("udp_recv.recvfrom");
        return JK_ERROR;
    }

    memset(address, 0, sizeof(*address));

    if (peer_addr.ss_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in *)&peer_addr;
        address->af = AF_INET;
        address->src_port = ntohs(p->sin_port);
        address->src.src_v4 = p->sin_addr;
    } else if (peer_addr.ss_family == AF_INET6) {
        struct sockaddr_in6 *p = (struct sockaddr_in6 *)&peer_addr;
        address->af = AF_INET6;
        address->src_port = ntohs(p->sin6_port);
        address->src.src_v6 = p->sin6_addr;
    } else {
        PANIC("Unsupported address family");
        return JK_ERROR;
    }

    return n;
}
