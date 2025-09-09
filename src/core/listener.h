#pragma once

#include "decl.h"

struct listener_s {
    int64_t fd;

    event_t *accept;

    uint32_t bound:1;
    uint32_t listening:1;
    uint32_t non_blocking:1;
    uint32_t error:1;
};

listener_t* make_listener(settings_t *s);
void release_listener(listener_t* l);
void accept_handler(event_t* ev);
