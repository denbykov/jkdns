#include "echo_handler.h"

#include <core/decl.h>
#include <core/event.h>
#include <core/io.h>
#include <core/buffer.h>
#include <core/connection.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void handle_echo_read(event_t *ev);
static void handle_echo_write(event_t *ev);

#define NET_BUFFER_SIZE 4096

void handle_echo(event_t *ev) {
    if (ev->owner.tag != EV_OWNER_CONNECTION) {
        fprintf(stderr, "handle_echo: bad event owner\n"); // NOLINT
        exit(1);
    }

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "handle_echo: event owner is NULL\n"); // NOLINT
        exit(1);
    }

    connection_t* conn = ev->owner.ptr;

    if (conn->data == NULL) {
        buffer_t* buf = calloc(1, sizeof(buffer_t));
        buf->data = calloc(NET_BUFFER_SIZE, sizeof(*buf->data));
        buf->capacity = NET_BUFFER_SIZE;
        buf->taken = 0;

        if (buf == NULL || buf->data == NULL) {
            perror("handle_hello.allocate_buffer");
            exit(1);
        }
        conn->data = buf;
    } 

    if (ev->write) {
        handle_echo_write(ev);
    } else {
        handle_echo_read(ev);
    }
}

void handle_echo_read(event_t *ev) {
    connection_t* conn = ev->owner.ptr;

    recv_buf(conn, conn->data);
}

void handle_echo_write(event_t *ev) {
    (void)ev;
}