#include "echo_proxy_handler.h"
#include "logger/logger.h"
#include "core/errors.h"
#include "session/tcp.h"
#include "settings/settings.h"

#include "core/decl.h"
#include "core/event.h"
#include "core/net.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/ev_backend.h"
#include <session/tcp.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void handle_echo_read(event_t *ev);
static void handle_echo_write(event_t *ev);
static void stop_echo_proxy(event_t *ev);

#define NET_BUFFER_SIZE 4096

typedef struct {
    connection_t *client;
    connection_t *remote;
    buffer_t* to_remote;
    buffer_t* to_client;
} state_t;

static void do_echo_read(
    event_t *ev, 
    connection_t* conn, buffer_t* buf, 
    connection_t* other);

static void do_echo_write(
    event_t *ev, 
    connection_t* conn, buffer_t* buf);

void handle_echo_proxy(event_t *ev) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(ev->owner.tag == EV_OWNER_CONNECTION, "bad event owner");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");
    
    connection_t* conn = ev->owner.ptr;

    if (conn->error) {
        log_perror("handle_echo_proxy");
        return stop_echo_proxy(ev);
    }
    
    if (conn->data == NULL) {
        settings_t *s = current_settings;

        state_t *state = calloc(1, sizeof(state_t));
        state->client = conn;
        conn->data = state;

        if (state == NULL) {
            log_perror("handle_echo_proxy.allocate_state");
            return stop_echo_proxy(ev);
        }

        connection_t *remote = tcp_connect(s->remote_ip, s->remote_port);
        state->remote = remote;
        remote->data = state;

        buffer_t* to_remote = calloc(1, sizeof(buffer_t));
        to_remote->data = calloc(NET_BUFFER_SIZE, sizeof(*to_remote->data));
        to_remote->capacity = NET_BUFFER_SIZE;
        to_remote->taken = 0;

        state->to_remote = to_remote;

        if (to_remote == NULL || to_remote->data == NULL) {
            log_perror("handle_echo_proxy.allocate_to_remote_buffer");
            return stop_echo_proxy(ev);
        }
        
        buffer_t* to_client = calloc(1, sizeof(buffer_t));
        to_client->data = calloc(NET_BUFFER_SIZE, sizeof(*to_client->data));
        to_client->capacity = NET_BUFFER_SIZE;
        to_client->taken = 0;

        state->to_client = to_client;

        if (to_client == NULL || to_client->data == NULL) {
            log_perror("handle_echo_proxy.allocate_to_client_buffer");
            return stop_echo_proxy(ev);
        }
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
    
    bool client_read = conn == state->client;

    if (client_read) {
        return do_echo_read(ev, conn, state->to_remote, state->remote);
    } else {
        return do_echo_read(ev, conn, state->to_client, state->client);
    }
}

void do_echo_read(
    event_t *ev, 
    connection_t* conn, buffer_t* buf, 
    connection_t* other) {
    logger_t *logger = current_logger;

    size_t space_left = buf->capacity - buf->taken;
    uint8_t* pos = buf->data + buf->taken * sizeof(*(buf->data));
    
    ssize_t read = recv_buf(conn, pos, space_left);

    if (read == 0) {
        log_trace("peer closed the connection");
        return stop_echo_proxy(ev);
    }
    
    if (read < 0 && read == JK_OUT_OF_BUFFER) {
        log_error("do_echo_read: no space left to read data into");
        return stop_echo_proxy(ev);
    }

    if (read < 0 && read == JK_ERROR) {
        log_perror("do_echo_read");
        return stop_echo_proxy(ev);
    }
    
    buf->taken += read;
    
    ev_backend->disable_event(conn->read);
    ev_backend->enable_event(other->write);
}

void handle_echo_write(event_t *ev) {
    connection_t* conn = ev->owner.ptr;
    state_t *state = (state_t*)conn->data;
    
    bool client_write = conn == state->client;

    if (client_write) {
        return do_echo_write(ev, conn, state->to_client);
    } else {
        return do_echo_write(ev, conn, state->to_remote);
    }
}

void do_echo_write(
    event_t *ev, 
    connection_t* conn, buffer_t* buf) {
    logger_t *logger = current_logger;

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    if (sent == 0) {
        log_trace("peer closed the connection");
        return stop_echo_proxy(ev);
    }

    if (sent == JK_ERROR) {
        log_perror("do_echo_write");
        return stop_echo_proxy(ev);
    }

    buf->taken = buf->taken - sent;

    if (buf->taken != 0) {
        memmove(buf->data, buf->data + sent * sizeof(*(buf->data)), buf->taken); // NOLINT
        return;
    }

    ev_backend->disable_event(conn->write);
    ev_backend->enable_event(conn->read);
}

void stop_echo_proxy(event_t* ev) {
    logger_t *logger = current_logger;
    log_trace("stopping proxy echo");

    connection_t* conn = ev->owner.ptr;

    state_t* state = conn->data;

    connection_t* client = state->client;
    connection_t* remote = state->remote;

    ev_backend->del_conn(client);
    close_connection(client);

    if (remote != NULL) {
        ev_backend->del_conn(remote);
        close_connection(remote);
    }

    if (state->to_remote != NULL) {
        buffer_t* to_remote = state->to_remote;

        if (to_remote != NULL && to_remote->data != NULL) {
            free(to_remote->data);
        }

        if (to_remote != NULL) {
            free(to_remote);
        }
    }

    if (state->to_client != NULL) {
        buffer_t* to_client = state->to_client;

        if (to_client != NULL && to_client->data != NULL) {
            free(to_client->data);
        }

        if (to_client != NULL) {
            free(to_client);
        }
    }

    free(state);
}
