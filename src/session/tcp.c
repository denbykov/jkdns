#include "tcp.h"
#include "core/decl.h"

#include <core/connection.h>
#include <core/event.h>
#include <core/ev_backend.h>

#include <echo/echo_handler.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void handle_new_connection(int64_t fd) {
    connection_t* conn = NULL;
    event_t* r_event = NULL;
    event_t* w_event = NULL;
    
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

    w_event = calloc(1, sizeof(event_t));
    if (conn == NULL) {
        perror("handle_new_connection.allocate_write_event");
        goto cleanup;
    }

    conn->fd = fd;
    conn->read  = r_event;
    conn->write = w_event;

    r_event->owner.tag = EV_OWNER_CONNECTION;
    r_event->owner.ptr = conn;
    r_event->write = false;
    r_event->handler = handle_echo;

    w_event->owner.tag = EV_OWNER_CONNECTION;
    w_event->owner.ptr = conn;
    w_event->write = true;
    w_event->handler = handle_echo;

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
