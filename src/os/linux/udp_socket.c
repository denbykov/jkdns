#include "core/ht.h"
#include "core/htt.h"
#include "core/udp_wq.h"
#include "logger/logger.h"
#include "core/decl.h"
#include "core/udp_socket.h"
#include "settings/settings.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>

#define LISTEN_QUEUE 10

udp_socket_t* make_udp_socket() {
    settings_t *s = current_settings;
    logger_t *logger = current_logger;

    udp_socket_t* sock = calloc(1, sizeof(udp_socket_t));
    if (sock == NULL) {
        log_perror("make_udp_socket.allocate_event_list");
        return NULL;
    }

    sock->connections = connection_ht_create(128);
    if (sock->connections == NULL) {
        log_perror("make_udp_socket.allocate_connections_ht");
        sock->error = true;
        return sock;
    }

    sock->wq = udp_wq_create(128);
    if (sock->wq == NULL) {
        log_perror("make_udp_socket.allocate_write_queue");
        sock->error = true;
        return sock;
    }
    
    sock->ev = NULL;
    sock->fd = -1;
    sock->readable = false;
    sock->writable = false;
    
    struct sockaddr_in server_sockaddr;
    int yes = 1;
    int fd = 0;
    
    server_sockaddr.sin_family=AF_INET;
	server_sockaddr.sin_port = htons(s->port);
	server_sockaddr.sin_addr.s_addr=INADDR_ANY;
	memset(&(server_sockaddr.sin_zero),0,8);
    
    fd = socket(AF_INET, SOCK_DGRAM,0);
    if (fd < 0) {
        log_perror("make_udp_socket.socket");
        sock->error = true;
        return sock;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        log_perror("make_udp_socket.setsockopt");
        sock->error = true;
        return sock;
    }
    
    if (bind(fd, (struct sockaddr *)&server_sockaddr,sizeof(struct sockaddr)) < 0) {
        log_perror("make_udp_socket.bind");
        sock->error = true;
        return sock;
    }
    
    sock->fd = fd;
    sock->bound = true;

    buffer_t buf;
    buf.data = calloc(UDP_MSG_SIZE, sizeof(*buf.data));
    if (buf.data == NULL) {
        log_perror("make_udp_socket.allocate_buffer");
        sock->error = true;
        return sock;
    }
    buf.capacity = UDP_MSG_SIZE;
    buf.taken = 0;

    sock->last_read_buf = buf;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        log_perror("make_udp_socket.fcntl_get_flags");
        sock->error = true;
        return sock;
    };
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        log_perror("make_udp_socket.fcntl_set_non_blocking");
        sock->error = true;
        return sock;
    }

    sock->non_blocking = true;

    sock->timer = NULL;

    return sock;
}

udp_socket_t* make_client_udp_socket() {
    logger_t *logger = current_logger;

    udp_socket_t* sock = calloc(1, sizeof(udp_socket_t));
    if (sock == NULL) {
        log_perror("make_client_udp_socket.allocate_event_list");
        return NULL;
    }

    sock->connections = connection_ht_create(128);
    if (sock->connections == NULL) {
        log_perror("make_client_udp_socket.allocate_connections_ht");
        sock->error = true;
        return sock;
    }

    sock->wq = udp_wq_create(128);
    if (sock->wq == NULL) {
        log_perror("make_client_udp_socket.allocate_write_queue");
        sock->error = true;
        return sock;
    }
    
    sock->ev = NULL;
    sock->fd = -1;
    sock->readable = false;
    sock->writable = false;
    
    int yes = 1;
    int fd = 0;
    
    fd = socket(AF_INET, SOCK_DGRAM,0);
    if (fd < 0) {
        log_perror("make_client_udp_socket.socket");
        sock->error = true;
        return sock;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        log_perror("make_client_udp_socket.setsockopt");
        sock->error = true;
        return sock;
    }
    
    sock->fd = fd;
    sock->bound = false;

    buffer_t buf;
    buf.data = calloc(UDP_MSG_SIZE, sizeof(*buf.data));
    if (buf.data == NULL) {
        log_perror("make_client_udp_socket.allocate_buffer");
        sock->error = true;
        return sock;
    }
    buf.capacity = UDP_MSG_SIZE;
    buf.taken = 0;

    sock->last_read_buf = buf;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        log_perror("make_client_udp_socket.fcntl_get_flags");
        sock->error = true;
        return sock;
    };
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        log_perror("make_client_udp_socket.fcntl_set_non_blocking");
        sock->error = true;
        return sock;
    }

    sock->non_blocking = true;

    sock->timer = NULL;

    return sock;
}

void release_udp_socket(udp_socket_t *sock) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(sock != NULL, "sock is NULL");

    if (sock->last_read_buf.data != NULL) {
        free(sock->last_read_buf.data);
    }

    close((int)sock->fd);

    if (sock->wq != NULL) {
        udp_wq_destroy(sock->wq);
    }

    if (sock->connections != NULL) {
        connection_ht_destroy(sock->connections);
    }

    free(sock);
}
