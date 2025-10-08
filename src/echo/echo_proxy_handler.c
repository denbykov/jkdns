#include "echo_proxy_handler.h"
#include "core/time.h"
#include "logger/logger.h"
#include "core/errors.h"
#include "connection/connection.h"
#include "settings/settings.h"

#include "core/decl.h"
#include "core/event.h"
#include "core/net.h"
#include "core/buffer.h"
#include "core/ev_backend.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void handle_echo_read(event_t *ev);
static void handle_echo_write(event_t *ev);
static void stop_echo_proxy(connection_t* conn);
static void abort_echo_proxy(connection_t* conn);

#define NET_BUFFER_SIZE 4096
#define ECHO_TIMEOUT 5000
#define ECHO_REMOTE_TIMEOUT 6000

typedef struct {
    connection_t *client;
    connection_t *remote;
    buffer_t* to_remote;
    buffer_t* to_client;

    jk_timer_t* timer;
    jk_timer_t* remote_timer;
} echo_context_t;

static jk_timer_t* start_new_timer(int timeout, connection_t* conn);
static void reschedule_timer(jk_timer_t* timer, int timeout, connection_t* conn);

static echo_context_t* create_context(connection_t* client);
static void destroy_context(echo_context_t* ctx);
static buffer_t* allocate_buffer();
static void destroy_buffer(buffer_t* buf);

static void do_echo_read(
    connection_t* conn, buffer_t* buf, 
    connection_t* other);

static void do_echo_write(
    connection_t* conn, buffer_t* buf);

buffer_t* allocate_buffer() {
    logger_t *logger = current_logger;
    buffer_t* buf = calloc(1, sizeof(buffer_t));
    if (buf == NULL) {
        log_perror("handle_echo_proxy.allocate_buffer");
        return NULL;
    }

    buf->data = calloc(NET_BUFFER_SIZE, sizeof(*buf->data));
    if (buf->data == NULL) {
        free(buf);
        log_perror("handle_echo_proxy.allocate_buffer_data");
        return NULL;
    }

    buf->capacity = NET_BUFFER_SIZE;
    buf->taken = 0;

    return buf;
}

void destroy_buffer(buffer_t* buf) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(buf != NULL, "buf is NULL!");
    CHECK_INVARIANT(buf->data != NULL, "buf->data is NULL!");

    free(buf->data);
    free(buf);
}

echo_context_t* create_context(connection_t* client) {
    logger_t *logger = current_logger;

    settings_t *s = current_settings;

    echo_context_t* ctx = calloc(1, sizeof(echo_context_t));
    if (ctx == NULL) {
        log_perror("handle_hello_proxy.create_context.allocate_context");
        return NULL;
    }

    ctx->client = client;

    void (*read_handler)(event_t *ev) = NULL;
    void (*write_handler)(event_t *ev) = NULL;

    CHECK_INVARIANT(s->proxy_mode == true, "unextected non-proxy mode call");

    read_handler = handle_echo_proxy;
    write_handler = handle_echo_proxy;

    connection_t *remote = tcp_connect(
        s->remote_ip,
        s->remote_port,
        read_handler,
        write_handler);
    
    ctx->remote = remote;
    remote->data = ctx;

    buffer_t* to_remote = allocate_buffer();
    if (to_remote == NULL) {
        close_connection(remote);
        free(ctx);
        log_error("handle_echo_proxy.allocate_to_remote_buffer");
        return NULL;
    }

    ctx->to_remote = to_remote;

    buffer_t* to_client = allocate_buffer();
    if (to_client == NULL) {
        destroy_buffer(to_remote);
        close_connection(remote);
        free(ctx);
        log_error("handle_echo_proxy.allocate_to_client_buffer");
        return NULL;
    }

    ctx->to_client = to_client;

    ctx->timer = NULL;
    ctx->remote_timer = NULL;

    return ctx;
}

void destroy_context(echo_context_t* ctx) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(ctx != NULL, "ctx is NULL!");
    CHECK_INVARIANT(ctx->timer != NULL, "ctx->timer is NULL!");
    CHECK_INVARIANT(ctx->remote_timer != NULL, "ctx->remote_timer is NULL!");

    destroy_buffer(ctx->to_client);
    destroy_buffer(ctx->to_remote);

    free(ctx);
}

void handle_echo_proxy(event_t *ev) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(ev->owner.tag == EV_OWNER_CONNECTION, "bad event owner");
    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");
    
    connection_t* conn = ev->owner.ptr;
    
    if (conn->error) {
        log_perror("handle_echo_proxy");
        return stop_echo_proxy(conn);
    }
    
    if (conn->data == NULL) {
        echo_context_t* ctx = create_context(conn);
        if (ctx == NULL) {
            abort_echo_proxy(conn);
            return;
        }

        ctx->client->data = ctx;
        ctx->remote->data = ctx;

        ctx->timer = start_new_timer(ECHO_TIMEOUT, ctx->client);
        ctx->remote_timer = start_new_timer(ECHO_REMOTE_TIMEOUT, ctx->remote);
    }

    if (ev->write) {
        handle_echo_write(ev);
    } else {
        handle_echo_read(ev);
    }
}

void handle_echo_read(event_t *ev) {
    connection_t* conn = ev->owner.ptr;
    echo_context_t *ctx = (echo_context_t*)conn->data;
    
    bool client_read = conn == ctx->client;

    reschedule_timer(
        ctx->timer,
        ECHO_TIMEOUT, 
        ctx->client);

    reschedule_timer(
        ctx->remote_timer, 
        ECHO_REMOTE_TIMEOUT,
        ctx->remote);

    if (client_read) {
        return do_echo_read(conn, ctx->to_remote, ctx->remote);
    } else {
        return do_echo_read(conn, ctx->to_client, ctx->client);
    }
}

void do_echo_read(
    connection_t* conn, buffer_t* buf, 
    connection_t* other) {
    logger_t *logger = current_logger;

    size_t space_left = buf->capacity - buf->taken;
    uint8_t* pos = buf->data + buf->taken * sizeof(*(buf->data));
    
    ssize_t read = recv_buf(conn, pos, space_left);

    if (read == 0) {
        log_trace("peer closed the connection");
        return stop_echo_proxy(conn);
    }
    
    if (read < 0 && read == JK_OUT_OF_BUFFER) {
        log_error("do_echo_read: no space left to read data into");
        return stop_echo_proxy(conn);
    }

    if (read < 0 && read == JK_ERROR) {
        log_perror("do_echo_read");
        return stop_echo_proxy(conn);
    }
    
    buf->taken += read;
    
    ev_backend->disable_event(conn->read);
    ev_backend->enable_event(other->write);
}

void handle_echo_write(event_t *ev) {
    connection_t* conn = ev->owner.ptr;
    echo_context_t *ctx = (echo_context_t*)conn->data;
    
    bool client_write = conn == ctx->client;

    reschedule_timer(
        ctx->timer,
        ECHO_TIMEOUT, 
        ctx->client);

    reschedule_timer(
        ctx->remote_timer, 
        ECHO_REMOTE_TIMEOUT,
        ctx->remote);

    if (client_write) {
        return do_echo_write(conn, ctx->to_client);
    } else {
        return do_echo_write(conn, ctx->to_remote);
    }
}

void do_echo_write(
    connection_t* conn, buffer_t* buf) {
    logger_t *logger = current_logger;

    ssize_t sent = send_buf(conn, buf->data, buf->taken);

    if (sent == 0) {
        log_trace("peer closed the connection");
        return stop_echo_proxy(conn);
    }

    if (sent == JK_ERROR) {
        log_perror("do_echo_write");
        return stop_echo_proxy(conn);
    }

    buf->taken = buf->taken - sent;

    if (buf->taken != 0) {
        memmove(buf->data, buf->data + sent * sizeof(*(buf->data)), buf->taken); // NOLINT
        return;
    }

    ev_backend->disable_event(conn->write);
    ev_backend->enable_event(conn->read);
}

void abort_echo_proxy(connection_t* conn) {
    logger_t *logger = current_logger;
    log_trace("aborting echo proxy");

    CHECK_INVARIANT(conn != NULL, "conn is NULL!");

    ev_backend->del_conn(conn);
    close_connection(conn);
}

void stop_echo_proxy(connection_t* conn) {
    logger_t *logger = current_logger;
    log_trace("stopping echo proxy");

    CHECK_INVARIANT(conn != NULL, "conn is NULL!");
    
    echo_context_t* ctx = conn->data;
    
    CHECK_INVARIANT(ctx != NULL, "ctx is NULL!");
    CHECK_INVARIANT(ctx->client != NULL, "ctx->client is NULL!");
    CHECK_INVARIANT(ctx->remote != NULL, "ctx->remote is NULL!");
    
    connection_t* client = ctx->client;
    connection_t* remote = ctx->remote;

    ctx->timer->enabled = false;
    ctx->remote_timer->enabled = false;

    ev_backend->del_conn(client);
    close_connection(client);

    ev_backend->del_conn(remote);
    close_connection(remote);

    destroy_context(ctx);
}

void handle_echo_timeout(void* data) {
    logger_t *logger = current_logger;

    connection_t* conn = data;
    CHECK_INVARIANT(conn != NULL, "conn is NULL!");
    
    echo_context_t* ctx = conn->data;
    CHECK_INVARIANT(ctx != NULL, "ctx is NULL!");

    bool is_client_timeout = ctx->client == conn;

    log_trace("echo proxy timeout, client_timeout: %s", BOOL_TO_S(is_client_timeout));
    stop_echo_proxy(conn);
}

jk_timer_t* start_new_timer(int timeout, connection_t* conn) {
    jk_timer_t timer;
    jk_timer_start(&timer, timeout);
    timer.handler = handle_echo_timeout;
    timer.data = conn;

    return ev_backend->add_timer(timer);
}

void reschedule_timer(jk_timer_t* timer, int timeout, connection_t* conn) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(timer != NULL, "timer is NULL");
    CHECK_INVARIANT(timer->enabled == true, "timer is disabled");
    
    jk_timer_start(timer, timeout);
    timer->handler = handle_echo_timeout;
    timer->data = conn;
}
