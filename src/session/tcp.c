#include "tcp.h"
#include "core/decl.h"
#include "settings/settings.h"

#include <core/connection.h>
#include <core/event.h>
#include <core/ev_backend.h>
#include <core/io.h>

#include <echo/echo_handler.h>
#include <echo/echo_proxy_handler.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void handle_new_connection(int64_t fd) {
    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;

    settings_t *s = current_settings;
    
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        perror("handle_new_connection.allocate_connection");
        goto cleanup;
    }

    r_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        perror("handle_new_connection.allocate_read_event");
        goto cleanup;
    }
    init_event(r_event);

    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        perror("handle_new_connection.allocate_write_event");
        goto cleanup;
    }
    init_event(w_event);

    conn->fd = fd;
    conn->read  = r_event;
    conn->write = w_event;

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

connection_t *tcp_connect(const char* ip, uint16_t port) {
    int64_t fd = open_tcp_conn(ip, port);

    settings_t *s = current_settings;

    if (fd == -1) {
        fprintf(stderr, "tcp_connect: failed to open tcp connection\n");
        exit(1);
    }

    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;
    
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        perror("tcp_connect.allocate_connection");
        goto cleanup;
    }

    r_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        perror("tcp_connect.allocate_read_event");
        goto cleanup;
    }
    init_event(r_event);

    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        perror("tcp_connect.allocate_write_event");
        goto cleanup;
    }
    init_event(w_event);

    conn->fd = fd;
    conn->read  = r_event;
    conn->write = w_event;

    void (*handler)(event_t *ev) = NULL;
    if (!s->proxy_mode) {
        fprintf(stderr, "tcp_connect: unable to chose handler for non-proxy mode\n");
        goto cleanup;
    }
    handler = handle_echo_proxy;

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
    close_tcp_conn(fd);

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
    close_tcp_conn(conn->fd);

    free(conn->read);
    free(conn->write);
    free(conn);
}
