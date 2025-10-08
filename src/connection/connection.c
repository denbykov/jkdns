#include "connection.h"
#include "core/decl.h"
#include "core/errors.h"
#include "settings/settings.h"

#include "core/connection.h"
#include "core/event.h"
#include "core/ev_backend.h"
#include "core/net.h"
#include "logger/logger.h"

#include <echo/echo_handler.h>
#include <echo/echo_proxy_handler.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

void handle_new_tcp_connection(int64_t fd) {
    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;

    settings_t *s = current_settings;
    logger_t *logger = current_logger;
    
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        log_perror("handle_new_tcp_connection.calloc");
        goto cleanup;
    }

    r_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("handle_new_tcp_connection.allocate_read_event");
        goto cleanup;
    }
    init_event(r_event);
    
    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("handle_new_tcp_connection.allocate_write_event");
        goto cleanup;
    }
    init_event(w_event);

    conn->handle.type = CONN_TYPE_TCP;
    conn->handle.data.fd = fd;

    conn->read  = r_event;
    conn->write = w_event;
    conn->error = false;

    void (*handler)(event_t *ev) = s->proxy_mode ? handle_echo_proxy: handle_echo;

    r_event->owner.tag = EV_OWNER_CONNECTION;
    r_event->owner.ptr = conn;
    r_event->write = false;
    r_event->handler = handler;
    
    w_event->owner.tag = EV_OWNER_CONNECTION;
    w_event->owner.ptr = conn;
    w_event->write = true;
    w_event->handler = handler;

    ev_backend->add_event(r_event);

    return;

    cleanup:
    if (conn != NULL) {
        free(conn);
    }

    if (r_event != NULL) {
        free(r_event);
    }

    if (w_event != NULL) {
        free(w_event);
    }

    exit(1);
}

connection_t* make_udp_connection(udp_socket_t* sock, address_t* address) {
    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;

    settings_t *s = current_settings;
    logger_t *logger = current_logger;
    
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        log_perror("make_udp_connection.calloc");
        goto cleanup;
    }

    memcpy(&conn->address, address, sizeof(*address));

    r_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("make_udp_connection.allocate_read_event");
        goto cleanup;
    }
    init_event(r_event);
    
    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("make_udp_connection.allocate_write_event");
        goto cleanup;
    }
    init_event(w_event);

    conn->handle.type = CONN_TYPE_UDP;
    conn->handle.data.sock = sock;

    conn->read  = r_event;
    conn->write = w_event;
    conn->error = false;

    void (*handler)(event_t *ev) = s->proxy_mode ? handle_echo_proxy: handle_echo;

    r_event->owner.tag = EV_OWNER_CONNECTION;
    r_event->owner.ptr = conn;
    r_event->write = false;
    r_event->handler = handler;
    
    w_event->owner.tag = EV_OWNER_CONNECTION;
    w_event->owner.ptr = conn;
    w_event->write = true;
    w_event->handler = handler;

    ev_backend->add_event(r_event);

    return conn;

    cleanup:
    if (conn != NULL) {
        free(conn);
    }

    if (r_event != NULL) {
        free(r_event);
    }

    if (w_event != NULL) {
        free(w_event);
    }

    exit(1);
}

connection_t *make_client_connection(
    conn_type_t type,
    const char* ip,
    uint16_t port,
    void (*read_handler)(event_t *ev), // NOLINT
    void (*write_handler)(event_t *ev)
) {
    logger_t *logger = current_logger;
    
    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;
    
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        log_perror("make_client_connection.allocate_connection");
        goto cleanup;
    }

    r_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("make_client_connection.allocate_read_event");
        goto cleanup;
    }
    init_event(r_event);

    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        log_perror("make_client_connection.allocate_write_event");
        goto cleanup;
    }
    init_event(w_event);
    
    int64_t res = fill_address(&conn->address, ip, port);

    CHECK_INVARIANT(res == JK_OK, "fill_address failed!");
    
    conn->handle.type = type;

    conn->read  = r_event;
    conn->write = w_event;

    r_event->owner.tag = EV_OWNER_CONNECTION;
    r_event->owner.ptr = conn;
    r_event->write = false;
    r_event->handler = read_handler;
    
    w_event->owner.tag = EV_OWNER_CONNECTION;
    w_event->owner.ptr = conn;
    w_event->write = true;
    w_event->handler = write_handler;

    res = ev_backend->add_conn(conn);
    if (res != JK_OK) {
        log_error("make_client_connection: add_conn failed");
        goto cleanup;
    } 

    return conn;

    cleanup:
    if (conn != NULL) {
        free(conn);
    }

    if (r_event != NULL) {
        free(r_event);
    }

    if (w_event != NULL) {
        free(w_event);
    }

    exit(1);
}

void close_connection(connection_t *conn) {
    logger_t *logger = current_logger;

    if (conn->handle.type == CONN_TYPE_TCP) {
        close_tcp_conn(conn->handle.data.fd);
    } else if (conn->handle.type == CONN_TYPE_UDP) {
        ;
    } else {
        PANIC("bad conneciton type");
    }

    free(conn->read);
    free(conn->write);
    free(conn);
}
