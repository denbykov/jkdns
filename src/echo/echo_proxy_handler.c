#include "echo_proxy_handler.h"
#include "session/tcp.h"
#include "settings/settings.h"

#include <core/decl.h>
#include <core/event.h>
#include <core/io.h>
#include <core/buffer.h>
#include <core/connection.h>
#include <core/ev_backend.h>
#include <session/tcp.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void handle_echo_read(event_t *ev);
static void handle_echo_write(event_t *ev);
static void handle_connection_closed(event_t *ev);

#define NET_BUFFER_SIZE 4096

typedef struct {
    connection_t *client;
    connection_t *remote;
    buffer_t* buf;
} state_t;

void handle_echo_proxy(event_t *ev) {
    if (ev->owner.tag != EV_OWNER_CONNECTION) {
        fprintf(stderr, "handle_echo_proxy: bad event owner\n");
        exit(1);
    }

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "handle_echo_proxy: event owner is NULL\n");
        exit(1);
    }
    
    connection_t* conn = ev->owner.ptr;
    
    if (conn->data == NULL) {
        settings_t *s = current_settings;

        state_t *state = calloc(1, sizeof(state_t));

        if (state == NULL) {
            perror("handle_echo_proxy.allocate_state");
            exit(1);
        }

        connection_t *r_conn = tcp_connect(s->remote_ip, s->remote_port);
        r_conn->data = state;

        buffer_t* buf = calloc(1, sizeof(buffer_t));
        buf->data = calloc(NET_BUFFER_SIZE, sizeof(*buf->data));
        buf->capacity = NET_BUFFER_SIZE;
        buf->taken = 0;

        state->client = conn;
        state->remote = r_conn;
        state->buf = buf;

        if (buf == NULL || buf->data == NULL) {
            perror("handle_echo_proxy.allocate_buffer");
            exit(1);
        }

        conn->data = state;
    }

    if (ev->write) {
        handle_echo_write(ev);
    } else {
        handle_echo_read(ev);
    }
}

void handle_echo_read(event_t *ev) {
    connection_t* conn = ev->owner.ptr;
    state_t *state = (state_t*)conn->data;
    buffer_t* buf = state->buf;

    connection_t *next_conn = (state->client == conn) ? state->remote : state->client;

    size_t space_left = buf->capacity - buf->taken;
    uint8_t* pos = buf->data + buf->taken * sizeof(*(buf->data));

    ssize_t read = recv_buf(conn, pos, space_left);

    if (read == 0) {
        handle_connection_closed(ev);
        return;
    }

    buf->taken += read;

    ev_backend->disable_event(conn->read);
    ev_backend->enable_event(next_conn->write);
}

void handle_echo_write(event_t *ev) {
    connection_t* conn = ev->owner.ptr;
    state_t *state = (state_t*)conn->data;
    buffer_t* buf = state->buf;

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    buf->taken = buf->taken - sent;

    if (buf->taken != 0) {
        memmove(buf->data, buf->data + sent * sizeof(*(buf->data)), buf->taken); //NOLINT
        return;
    }

    ev_backend->disable_event(conn->write);
    ev_backend->enable_event(conn->read);
}

void handle_connection_closed(event_t* ev) {
    connection_t* conn = ev->owner.ptr;

    ev_backend->del_event(conn->read);
    // ToDo: handle double deletion issue
    // ev_backend->del_event(conn->write);
    buffer_t *buf = (buffer_t*)conn->data;

    // ToDo: state has to be released

    if (buf != NULL && buf->data != NULL) {
        free(buf->data);
    }

    if (buf != NULL) {
        free(buf);
    }

    close_connection(conn);
}
