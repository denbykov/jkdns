#include "udp_socket.h"
#include "core/connection.h"
#include "core/decl.h"
#include "core/errors.h"
#include "core/ht.h"
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

        if (read == JK_EXHAUSTED) {
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
            conn = make_udp_connection(sock);
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

}

int64_t udp_add_event(event_t* ev, connection_t* conn) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_UDP, "Bad connection type");
    udp_socket_t *sock = conn->handle.data.sock;

    if (ev->write) {
        ev->enabled = true;
        // add to the queue
        if (sock->writable) {
            // handle_writes()
        }
    }

    if (!ev->write) {
        ev->enabled = true;
    }

    return JK_OK;
}

int64_t udp_del_event(event_t* ev, connection_t* conn) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(conn->handle.type == CONN_TYPE_UDP, "Bad connection type");

    if (ev->write) {
        // remove from the queue
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
