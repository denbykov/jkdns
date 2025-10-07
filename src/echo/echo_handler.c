#include "echo_handler.h"
#include "core/errors.h"
#include "core/ht.h"
#include "core/time.h"
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
static void handle_echo_timeout(void* data);

#define NET_BUFFER_SIZE 4096

typedef struct {
    buffer_t* buf;
    jk_timer_t* timer;
} echo_context_t;

static void start_new_timer(echo_context_t* ctx, int timeout, connection_t* conn);

static echo_context_t* create_context();
static void destroy_context(echo_context_t* ctx);

echo_context_t* create_context() {
    logger_t *logger = current_logger;

    echo_context_t* ctx = calloc(1, sizeof(echo_context_t));
    if (ctx == NULL) {
        log_perror("handle_hello.create_context.allocate_context");
        return NULL;
    }

    buffer_t* buf = calloc(1, sizeof(buffer_t));
    if (buf == NULL) {
        free(ctx);
        log_perror("handle_hello.create_context.allocate_buffer");
        return NULL;
    }

    buf->data = calloc(NET_BUFFER_SIZE, sizeof(*buf->data));
    if (buf->data == NULL) {
        free(ctx);
        free(buf);
        log_perror("handle_hello.create_context.allocate_buffer->data");
        return NULL;
    }
    buf->capacity = NET_BUFFER_SIZE;
    buf->taken = 0;
    ctx->buf = buf;
    ctx->timer = NULL;

    return ctx;
}

void destroy_context(echo_context_t* ctx) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(ctx != NULL, "ctx is NULL!");
    CHECK_INVARIANT(ctx->buf != NULL, "ctx->buf is NULL!");
    CHECK_INVARIANT(ctx->buf->data != NULL, "ctx->buf->data is NULL!");
    CHECK_INVARIANT(ctx->timer != NULL, "ctx->timer is NULL!");

    free(ctx->buf->data);
    free(ctx->buf);
    free(ctx);
}

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
        echo_context_t* ctx = create_context();

        if (ctx == NULL) {
            // ToDo: need better stop procedure
            return stop_echo(ev);
        }

        start_new_timer(ctx, 5000, conn);
        conn->data = ctx;
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
    echo_context_t* ctx = conn->data;
    buffer_t* buf = ctx->buf;

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

    // dirty trick to facilitate logging
    buf->data[buf->taken] = 0;
    log_trace("handle_echo_read.msg: %s", buf->data);

    ev_backend->disable_event(conn->read);
    ev_backend->enable_event(conn->write);

    start_new_timer(ctx, 5000, conn);
}

void handle_echo_write(event_t *ev) {
    logger_t *logger = current_logger;

    connection_t* conn = ev->owner.ptr;
    echo_context_t* ctx = conn->data;
    buffer_t* buf = ctx->buf;

    // dirty trick to facilitate logging
    buf->data[buf->taken] = 0;
    log_trace("handle_echo_write.msg: %s", buf->data);

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    if (sent == JK_WOULD_BLOCK) {
        return;
    }

    if (sent == 0) {
        log_trace("peer closed the connection");
        return stop_echo(ev);
    }

    if (sent < 0) {
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

    start_new_timer(ctx, 5000, conn);
}

void stop_echo(event_t* ev) {
    logger_t *logger = current_logger;

    log_trace("stopping echo");

    connection_t* conn = ev->owner.ptr;

    ev_backend->del_conn(conn);

    echo_context_t *ctx = (echo_context_t*)conn->data;
    if (ctx->timer != NULL) {
        ctx->timer->enabled = false;
    }
    
    destroy_context(ctx);
    
    close_connection(conn);
}

void handle_echo_timeout(void* data) {
    logger_t *logger = current_logger;

    connection_t *conn = data;

    CHECK_INVARIANT(conn != NULL, "conn is NULL!");

    log_trace("stopping echo due to the timeout");

    ev_backend->del_conn(conn);

    echo_context_t *ctx = (echo_context_t*)conn->data;
    if (ctx->timer != NULL) {
        ctx->timer->enabled = false;
    }
    
    destroy_context(ctx);

    close_connection(conn);
}

void start_new_timer(echo_context_t* ctx, int timeout, connection_t* conn) {
    logger_t *logger = current_logger;

    if (ctx->timer != NULL) {
        CHECK_INVARIANT(ctx->timer->enabled == true, "old timer is disabled");
        ctx->timer->enabled = false;
    }
    
    jk_timer_t timer;
    jk_timer_start(&timer, timeout);
    timer.handler = handle_echo_timeout;
    timer.data = conn;

    ctx->timer = ev_backend->add_timer(timer);
}
