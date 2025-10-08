#pragma once

#include <stdint.h>
#include <unistd.h>

#include "errors.h"

#define UDP_MSG_SIZE 512

#define BOOL_TO_S(arg) ((arg) ? "true" : "false")

typedef struct address_s address_t;
typedef struct connection_s connection_t;
typedef struct event_s event_t;
typedef struct ev_backend_s ev_backend_t;
typedef struct listener_s listener_t;
typedef struct udp_socket_s udp_socket_t;
typedef struct buffer_s buffer_t;
typedef struct settings_s settings_t;
