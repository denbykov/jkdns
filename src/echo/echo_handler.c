#include "echo_handler.h"
#include "logger/logger.h"

#include "core/decl.h"
#include "core/event.h"
#include "core/net.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/ev_backend.h"
#include "connection/connection.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void handle_echo_read(event_t *ev);
static void handle_echo_write(event_t *ev);
static void stop_echo(event_t *ev);

#define NET_BUFFER_SIZE 4096

void handle_echo(event_t *ev) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(ev->owner.tag == EV_OWNER_CONNECTION, "bad event owner");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");

    connection_t* conn = ev->owner.ptr;

    if (conn->error) {
        log_perror("handle_echo");
        return stop_echo(ev);
    }

    if (conn->data == NULL) {
        buffer_t* buf = calloc(1, sizeof(buffer_t));
        buf->data = calloc(NET_BUFFER_SIZE, sizeof(*buf->data));
        buf->capacity = NET_BUFFER_SIZE;
        buf->taken = 0;
        
        conn->data = buf;

        if (buf == NULL || buf->data == NULL) {
            log_perror("handle_hello.allocate_buffer");
            return stop_echo(ev);
        }
    } 

    if (ev->write) {
        handle_echo_write(ev);
    } else {
        handle_echo_read(ev);
    }
}

void handle_echo_read(event_t *ev) {
    logger_t *logger = current_logger;

    connection_t* conn = ev->owner.ptr;
    buffer_t* buf = (buffer_t*)conn->data;

    size_t space_left = buf->capacity - buf->taken;
    uint8_t* pos = buf->data + buf->taken * sizeof(*(buf->data));

    ssize_t read = recv_buf(conn, pos, space_left);

    if (read == 0) {
        log_trace("peer closed the connection");
        return stop_echo(ev);
    }
    
    if (read < 0 && read == JK_OUT_OF_BUFFER) {
        log_error("do_echo_read: no space left to read data into");
        return stop_echo(ev);
    }

    if (read < 0 && read == JK_ERROR) {
        log_perror("do_echo_read");
        return stop_echo(ev);
    }

    buf->taken += read;

    ev_backend->disable_event(conn->read);
    ev_backend->enable_event(conn->write);
}

void handle_echo_write(event_t *ev) {
    logger_t *logger = current_logger;

    connection_t* conn = ev->owner.ptr;
    buffer_t* buf = (buffer_t*)conn->data;

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    if (sent == 0) {
        log_trace("peer closed the connection");
        return stop_echo(ev);
    }

    if (sent == JK_ERROR) {
        log_perror("do_echo_write");
        return stop_echo(ev);
    }

    buf->taken = buf->taken - sent;

    if (buf->taken != 0) {
        memmove(buf->data, buf->data + sent * sizeof(*(buf->data)), buf->taken); // NOLINT
        return;
    }

    ev_backend->disable_event(conn->write);
    ev_backend->enable_event(conn->read);
}

void stop_echo(event_t* ev) {
    logger_t *logger = current_logger;

    log_trace("stopping echo");

    connection_t* conn = ev->owner.ptr;

    ev_backend->del_conn(conn);
    buffer_t *buf = (buffer_t*)conn->data;

    if (buf != NULL && buf->data != NULL) {
        free(buf->data);
    }

    if (buf != NULL) {
        free(buf);
    }

    close_connection(conn);
}
