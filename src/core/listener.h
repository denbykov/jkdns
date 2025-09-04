#pragma once

#include "decl.h"

struct listener_s {
    uint64_t fd;

    event_t *accept;

    uint32_t initialized:1;
    uint32_t bound:1;
    uint32_t non_blocking:1;
    uint32_t listening:1;
};

listener_t* (*make_listener)();
void (*release_listener)(listener_t* l);
