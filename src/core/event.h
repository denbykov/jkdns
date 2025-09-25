#pragma once

#include <stdint.h>

#include "decl.h"

typedef void (*event_handler_pt)(event_t* ev);

enum event_owner_tag {
    EV_OWNER_NONE,
    EV_OWNER_LISTENER,
    EV_OWNER_CONNECTION,
    EV_OWNER_USOCK,
};

typedef struct {
    void *ptr;
    enum event_owner_tag tag;
} event_owner_t;

struct event_s {
    event_owner_t owner;

    uint32_t write:1;
    uint32_t enabled:1;

    event_handler_pt handler;

    // ToDo: consider SMP optimization
};

void init_event(event_t *ev);
