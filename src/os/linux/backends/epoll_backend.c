#include "core/errors.h"
#include "core/time.h"
#include "logger/logger.h"
#include "core/decl.h"
#include "core/event.h"
#include "core/ev_backend.h"
#include "core/listener.h"
#include "core/connection.h"
#include "core/udp_socket.h"
#include "core/net.h"
#include "udp_socket/udp_socket.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/epoll.h>

#define EPOLL_MAX_EVENTS 512

static struct epoll_event* event_list;
static int epoll_fd = -1;
static jk_timer_heap_t* epoll_th = NULL;

static int64_t epoll_init();
static int64_t epoll_shutdown();
static int64_t epoll_add_event(event_t* ev);
static int64_t epoll_del_event(event_t* ev);
static int64_t epoll_enable_event(event_t* ev);
static int64_t epoll_disable_event(event_t* ev);
static int64_t epoll_add_conn(connection_t* conn);
static int64_t epoll_del_conn(connection_t* conn);
static int64_t epoll_add_udp_sock(udp_socket_t* sock);
static int64_t epoll_del_udp_sock(udp_socket_t* sock);
static void epoll_register_time_heap(jk_timer_heap_t* th);
static int64_t epoll_process_events();
static int64_t epoll_process_timers();
static jk_timer_t* epoll_add_timer(jk_timer_t timer);

ev_backend_t epoll_backend = {
    .name = "epoll",
    .init = epoll_init,
    .shutdown = epoll_shutdown,
    .add_event = epoll_add_event,
    .del_event = epoll_del_event,
    .enable_event = epoll_enable_event,
    .disable_event = epoll_disable_event,
    .add_conn = epoll_add_conn,
    .del_conn = epoll_del_conn,
    .add_udp_sock = epoll_add_udp_sock,
    .del_udp_sock = epoll_del_udp_sock,
    .register_time_heap = epoll_register_time_heap,
    .process_events = epoll_process_events,
    .process_timers = epoll_process_timers,
    .add_timer = epoll_add_timer
};

static int64_t epoll_init() {
    logger_t* logger = current_logger;

    event_list = calloc(EPOLL_MAX_EVENTS, sizeof(struct epoll_event));
    if (event_list == NULL) {
        log_perror("epoll_init.allocate_event_list");
        return JK_ERROR;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        log_perror("epoll_init.epoll_create1");
        return JK_ERROR;
    }

    return JK_OK;
}

static int64_t epoll_shutdown() {
    if (event_list != NULL) {
        free(event_list);
    }

    if (epoll_fd != -1) {
        close(epoll_fd);
    }

    return JK_OK;
}

static int64_t epoll_add(event_t* ev, int64_t fd) {
    logger_t* logger = current_logger;

    struct epoll_event event;
    if (ev->write) {
        event.events = EPOLLOUT | EPOLLET;
    } else {
        event.events = EPOLLIN | EPOLLET;
    }

    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) { // NOLINT
        log_perror("epoll_add.epoll_ctl");
        return JK_ERROR;
    }

    ev->enabled = true;

    return JK_OK;
}

static int64_t epoll_add_event(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->enabled == false, "event is already enabled");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");

    if (ev->owner.tag == EV_OWNER_LISTENER) {
        return epoll_add(ev, ((listener_t*)ev->owner.ptr)->fd);
    }

    if (ev->owner.tag == EV_OWNER_CONNECTION) {
        connection_t *conn = ev->owner.ptr;

        if (conn->handle.type == CONN_TYPE_TCP) {
            return epoll_add(ev, conn->handle.data.fd);
        } else if (conn->handle.type == CONN_TYPE_UDP) {
            return udp_add_event(ev, conn);
        } else {
            PANIC("bad connection type");
        }
    }
    
    PANIC("unknown event owner");
    return JK_ERROR;
}

static int64_t epoll_del(event_t* ev, int64_t fd) {
    logger_t* logger = current_logger;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) { // NOLINT
        log_perror("epoll_del.epoll_ctl");
        return JK_ERROR;
    }

    ev->enabled = false;

    return JK_OK;
}

static int64_t epoll_del_event(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");

    if (ev->owner.tag == EV_OWNER_LISTENER) {
        return epoll_del(ev, ((listener_t*)ev->owner.ptr)->fd);
    }

    if (ev->owner.tag == EV_OWNER_CONNECTION) {
        connection_t *conn = ev->owner.ptr;

        if (conn->handle.type == CONN_TYPE_TCP) {
            return epoll_del(ev, conn->handle.data.fd);
        } else if (conn->handle.type == CONN_TYPE_UDP) {
            return udp_del_event(ev, conn);
        } else {
            PANIC("bad connection type");
        }
    }

    return JK_OK;
}

static int64_t epoll_enable(event_t* ev, int64_t fd) {
    logger_t* logger = current_logger;

    struct epoll_event event;
    if (ev->write) {
        event.events = EPOLLOUT | EPOLLET;
    } else {
        event.events = EPOLLIN | EPOLLET;
    }

    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) { // NOLINT
        log_perror("epoll_enable.epoll_ctl");
        return JK_ERROR;
    }

    ev->enabled = true;

    return JK_OK;
}

static int64_t epoll_enable_event(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->enabled == false, "event is already enabled");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");

    if (ev->owner.tag == EV_OWNER_LISTENER) {
        return epoll_enable(ev, ((listener_t*)ev->owner.ptr)->fd);
    }

    if (ev->owner.tag == EV_OWNER_CONNECTION) {
        connection_t *conn = ev->owner.ptr;

        if (conn->handle.type == CONN_TYPE_TCP) {
            return epoll_enable(ev, conn->handle.data.fd);
        } else if (conn->handle.type == CONN_TYPE_UDP) {
            return udp_enable_event(ev, conn);
        } else {
            PANIC("bad connection type");
        }
    }
    
    PANIC("unknown event owner");
    return JK_ERROR;
}

static int64_t epoll_disable(event_t* ev, int64_t fd) {
    logger_t* logger = current_logger;

    struct epoll_event event;
    event.events = 0;
    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) { // NOLINT
        log_perror("epoll_disable.epoll_ctl");
        return JK_ERROR;
    }

    ev->enabled = false;

    return JK_OK;
}

static int64_t epoll_disable_event(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->enabled == true, "event is already disabled");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");

    if (ev->owner.tag == EV_OWNER_LISTENER) {
        return epoll_disable(ev, ((listener_t*)ev->owner.ptr)->fd);
    }

    if (ev->owner.tag == EV_OWNER_CONNECTION) {
        connection_t *conn = ev->owner.ptr;

        if (conn->handle.type == CONN_TYPE_TCP) {
            return epoll_disable(ev, conn->handle.data.fd);
        } else if (conn->handle.type == CONN_TYPE_UDP) {
            return udp_disable_event(ev, conn);
        } else {
            PANIC("bad connection type");
        }
    }

    PANIC("unknown event owner");
    return JK_ERROR;
}

static int64_t epoll_add_conn(connection_t* conn) {
    logger_t* logger = current_logger;
    int64_t fd = 0;

    if (conn->handle.type == CONN_TYPE_TCP) {
        fd = open_tcp_conn(&conn->address);
        if (fd == JK_ERROR) {
            log_perror("epoll_add_conn: failed to open tcp connection");
            return JK_ERROR;
        }

        conn->handle.data.fd = fd;
    } else if (conn->handle.type == CONN_TYPE_UDP) {
        PANIC("unimplemented");
    } else {
        PANIC("bad connection type");
    }

    struct epoll_event event;
        
    event.events = 0;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) { // NOLINT
        log_perror("epoll_add_conn.epoll_ctl");
        return JK_ERROR;
    }

    return JK_OK;
}

static int64_t epoll_del_conn(connection_t* conn) {
    logger_t* logger = current_logger;
    int64_t fd = 0;

    conn->read->enabled = false;
    conn->write->enabled = false;

    if (conn->handle.type == CONN_TYPE_TCP) {
        fd = conn->handle.data.fd;
    } else if (conn->handle.type == CONN_TYPE_UDP) {
        return udp_del_connection(conn);
    } else {
        PANIC("bad connection type");
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) { // NOLINT
        log_perror("epoll_del_conn.epoll_ctl");
        return JK_ERROR;
    }

    return JK_OK;
}

void epoll_register_time_heap(jk_timer_heap_t* th) {
    epoll_th = th;
}

static int64_t epoll_process_events() {
    logger_t* logger = current_logger;

    // default timeout
    int timeout = 10000;
    jk_timer_t* next_timer = jk_th_peek(epoll_th);
    if (next_timer != NULL) {
        int until_timer_expiry = (int)(next_timer->expiry - jk_now());
        if (until_timer_expiry < timeout) {
            timeout = until_timer_expiry;
        }
    }

    // ToDo: add timeout
    int nfds = epoll_wait(
        epoll_fd,
        event_list,
        EPOLL_MAX_EVENTS,
        timeout);
    if (nfds == -1) {
        log_perror("epoll_process_events.epoll_wait");
        return JK_ERROR;
    }

    for (int n = 0; n < nfds; ++n) {
        CHECK_INVARIANT(event_list[n].data.ptr != NULL, "event is NULL");

        event_t* ev = (event_t*)event_list[n].data.ptr;

        if (event_list[n].events & (EPOLLERR | EPOLLHUP)) {
            int fd = 0;

            log_trace("epoll_process_events: event failure detected");

            switch (ev->owner.tag) {
                case EV_OWNER_LISTENER:
                    ((listener_t*)ev->owner.ptr)->error = true;
                    fd = ((listener_t*)ev->owner.ptr)->fd; // NOLINT
                    break;
                case EV_OWNER_CONNECTION: {
                    connection_t *conn = ev->owner.ptr;
                    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_TCP, "Should never happen");
                    conn->error = true;
                    fd = conn->handle.data.fd; // NOLINT
                    break;
                }
                case EV_OWNER_USOCK:
                    ((udp_socket_t*)ev->owner.ptr)->error = true;
                    fd = ((udp_socket_t*)ev->owner.ptr)->fd; // NOLINT
                    break;
                default:
                    PANIC("unknown event owner");
            }

            int err = 0;
            socklen_t len = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
                PANIC("failed to getsockopt");
            }

            errno = err;
        }

        if (ev->owner.tag == EV_OWNER_USOCK && event_list[n].events & EPOLLIN) {
            udp_socket_t* sock = ev->owner.ptr;
            sock->readable = true;
        }

        if (ev->owner.tag == EV_OWNER_USOCK && event_list[n].events & EPOLLOUT) {
            udp_socket_t* sock = ev->owner.ptr;
            sock->writable = true;
        }

        CHECK_INVARIANT(ev->handler != NULL, "event handler is NULL");

        ev->handler(ev);
    }

    return JK_OK;
}

int64_t epoll_process_timers() {
    logger_t* logger = current_logger;

    // log_trace("epoll_process_timers start");
    // jk_th_debug_dump(epoll_th);
    
    int64_t now = jk_now();
    
    for (;;) {
        jk_timer_t* timer = jk_th_peek(epoll_th);
        if (timer == NULL) {
            break;
        }
        
        CHECK_INVARIANT(timer->handler != NULL, "timer handler is NULL");
        
        if (!timer->enabled) {
            // log_debug("discrarding disabled timer");
            jk_th_pop(epoll_th);
            continue;
        } else if (timer->expiry <= now) {
            // log_debug("timer expired!");
            timer->handler(timer->data);
            jk_th_pop(epoll_th);
            continue;
        } else {
            break;
        }
    }
    
    // log_trace("epoll_process_timers end");

    return JK_OK;
}

static int64_t epoll_add_udp_sock(udp_socket_t* sock) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(sock->ev != false, "event is NULL");
    CHECK_INVARIANT(sock->ev->enabled == false, "event is already enabled");

    int64_t fd = sock->fd;
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = sock->ev;
    sock->ev->enabled = true;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) { // NOLINT
        log_perror("epoll_add_udp_sock.epoll_ctl");
        return JK_ERROR;
    }

    return JK_OK;
}

static int64_t epoll_del_udp_sock(udp_socket_t* sock) {
    logger_t* logger = current_logger;

    int64_t fd = sock->fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) { // NOLINT
        log_perror("epoll_del_udp_sock.epoll_ctl");
        return JK_ERROR;
    }
    
    sock->ev->enabled = false;

    return JK_OK;
}

static jk_timer_t* epoll_add_timer(jk_timer_t timer) {
    return jk_th_add(epoll_th, timer);
}
