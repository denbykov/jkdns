#include "echo_proxy_handler.h"
#include "session/tcp.h"
#include "settings/settings.h"

#include <core/decl.h>
#include <core/event.h>
#include <core/net.h>
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
    connection_t* conn, buffer_t* buf, 
    connection_t* other);

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

    if (conn->error) {
        perror("handle_echo_proxy");
        stop_echo_proxy(ev);
        return;
    }
    
    if (conn->data == NULL) {
        settings_t *s = current_settings;

        state_t *state = calloc(1, sizeof(state_t));
        state->client = conn;
        conn->data = state;

        if (state == NULL) {
            perror("handle_echo_proxy.allocate_state");
            exit(1);
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
            perror("handle_echo_proxy.allocate_to_remote_buffer");
            exit(1);
        }
        
        buffer_t* to_client = calloc(1, sizeof(buffer_t));
        to_client->data = calloc(NET_BUFFER_SIZE, sizeof(*to_client->data));
        to_client->capacity = NET_BUFFER_SIZE;
        to_client->taken = 0;

        state->to_client = to_client;

        if (to_client == NULL || to_client->data == NULL) {
            perror("handle_echo_proxy.allocate_to_client_buffer");
            exit(1);
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
    size_t space_left = buf->capacity - buf->taken;
    uint8_t* pos = buf->data + buf->taken * sizeof(*(buf->data));
    
    ssize_t read = recv_buf(conn, pos, space_left);
    
    if (read <= 0) {
        stop_echo_proxy(ev);
        return;
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
        return do_echo_write(ev, conn, state->to_client, state->remote);
    } else {
        return do_echo_write(ev, conn, state->to_remote, state->client);
    }
}

void do_echo_write(
    event_t *ev, 
    connection_t* conn, buffer_t* buf, 
    connection_t* other) {
    (void)ev; // unused for now
    (void)other; // unused for now

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    buf->taken = buf->taken - sent;

    if (buf->taken != 0) {
        memmove(buf->data, buf->data + sent * sizeof(*(buf->data)), buf->taken); // NOLINT
        return;
    }

    ev_backend->disable_event(conn->write);
    ev_backend->enable_event(conn->read);
}

void stop_echo_proxy(event_t* ev) {
    printf("closing connections\n");

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
