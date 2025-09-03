#pragma once

#include "decl.h"

#define add_event       event_actions.add
#define del_event       event_actions.del
#define process_events  event_actions.process

typedef struct {
    int64_t (*add)(event_t* ev);
    int64_t (*del)(event_t* ev);

    int64_t (*process)();
} event_actions_t;

struct ev_backend_s {
    char* name;

    event_actions_t actions;
};
