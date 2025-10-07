#include "udp_socket.h"
#include "core/connection.h"
#include "core/decl.h"
#include "core/errors.h"
#include "core/ht.h"
#include "core/htt.h"
#include "core/udp_wq.h"
#include "logger/logger.h"
#include "core/event.h"
#include "core/net.h"
#include "connection/connection.h"
#include <stdint.h>

static void handle_reads(udp_socket_t* sock);
static void handle_writes(udp_socket_t* sock);

void udp_ev_handler(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->owner.tag == EV_OWNER_USOCK, "event owner is not a udp socket");
    udp_socket_t* sock = ev->owner.ptr;

    CHECK_INVARIANT(sock->readable || sock->writable, "udp socket is neither writable or readable");
    
    if (sock->readable) {
        handle_reads(sock);
    }

    if (sock->writable) {
        handle_writes(sock);
    }
}

static void handle_reads(udp_socket_t* sock) {
    logger_t* logger = current_logger;

    address_t address;
    buffer_t *buf = &sock->last_read_buf;
    connection_ht_t* ht = sock->connections;

    for (;;) {
        ssize_t read = udp_recv(
            sock,
            buf->data,
            buf->capacity,
            &address);

        CHECK_INVARIANT(read != 0, "should never happen");

        if (read == JK_WOULD_BLOCK) {
            break;
        }
        
        if (read == JK_ERROR) {
            // ToDo: may lead to endless loop, consider fixing
            log_warn("handle_reads: read failure");
            continue;
        }

        buf->taken = read;
        buf->capacity -= read;

        connection_t* conn = connection_ht_lookup(ht, &address);
        if (conn == NULL) {
            log_trace("handle_reads: new conn");
            conn = make_udp_connection(sock, &address);
            connection_ht_insert(ht, &address, conn);
        } else {
            log_trace("handle_reads: existing conn");
        }
        
        if (conn->read->enabled) {
            conn->read->handler(conn->read);
        } else {
            log_warn("handle_reads: discarding data for unarmed event");
        }
    }
}

static void handle_writes(udp_socket_t* sock) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(sock->writable, "udp socket is not writable!");
    
    udp_wq_t* wq = sock->wq;
    
    for (;;) {
        event_t* ev = udp_wq_pop_front(wq);
        if (ev == NULL) {
            break;
        }
        
        CHECK_INVARIANT(ev->enabled, "write event is not enabled!");

        ev->handler(ev);

        if (!sock->writable) {
            break;
        } 
    } 
}

int64_t udp_add_event(event_t* ev, connection_t* conn) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_UDP, "bad connection type");
    udp_socket_t *sock = conn->handle.data.sock;
    udp_wq_t* wq = sock->wq;

    if (ev->write) {
        int res = udp_wq_add(wq, ev);
        if (res == JK_OUT_OF_BUFFER) {
            return JK_ERROR;
        }

        ev->enabled = true;
        if (sock->writable) {
            handle_writes(sock);
        }
    }

    if (!ev->write) {
        ev->enabled = true;
    }

    return JK_OK;
}

int64_t udp_del_event(event_t* ev, connection_t* conn) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_UDP, "bad connection type");
    udp_socket_t *sock = conn->handle.data.sock;
    udp_wq_t* wq = sock->wq;
    
    if (ev->write) {
        int res = udp_wq_remove(wq, ev);
        CHECK_INVARIANT(res == JK_OK, "udp wq remove failed!");
        ev->enabled = false;
    }
    
    if (!ev->write) {
        ev->enabled = false;
    }
    
    return JK_OK;
}

int64_t udp_enable_event(event_t* ev, connection_t* conn) {
    return udp_add_event(ev, conn);
}

int64_t udp_disable_event(event_t* ev, connection_t* conn) {
    return udp_del_event(ev, conn);
}

int64_t udp_del_connection(connection_t *conn) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn != NULL, "conneciton is NULL");
    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_UDP, "bad connection type");

    udp_socket_t* sock = conn->handle.data.sock;

    int res = 0;
    
    res = udp_wq_remove(sock->wq, conn->write);
    CHECK_INVARIANT(res == JK_OK, "failed to remove event from wq");
    
    res = connection_ht_delete(sock->connections, &conn->address);
    CHECK_INVARIANT(res == JK_OK, "failed to remove connection from connections ht");

    return JK_OK;
}
