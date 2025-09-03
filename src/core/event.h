#pragma once

#include <stdint.h>

#include "decl.h"

typedef void (*event_handler_pt)(struct event_s* ev);

enum event_owner_tag {
    EV_OWNER_LISTENER,
    EV_OWNER_CONNECTION
};

typedef struct {
    void *ptr;
    enum event_owner_tag tag;
} event_owner_t;

struct event_s {
    event_owner_t owner;

    uint32_t write:1;
    // uint32_t accept:1;

    event_handler_pt handler;

    // ToDo: consider SMP optimization
};
