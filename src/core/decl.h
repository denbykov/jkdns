#pragma once

#include <stdint.h>
#include "errors.h"

typedef uint64_t ssize_t;

typedef struct connection_s connection_t;
typedef struct event_s event_t;
typedef struct ev_backend_s ev_backend_t;
typedef struct listener_s listener_t;
typedef struct buffer_s buffer_t;
typedef struct settings_s settings_t;
