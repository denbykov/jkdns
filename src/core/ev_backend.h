#pragma once

#include "decl.h"

struct ev_backend_s {
    const char* name;

    int64_t (*init)();
    int64_t (*shutdown)();

    int64_t (*add_event)(event_t* ev);
    int64_t (*del_event)(event_t* ev);

    int64_t (*process_events)();
};
