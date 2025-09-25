#pragma once

#include "decl.h"

struct ev_backend_s {
    const char* name;

    int64_t (*init)();
    int64_t (*shutdown)();

    int64_t (*add_event)(event_t* ev);
    int64_t (*del_event)(event_t* ev);

    int64_t (*enable_event)(event_t* ev);
    int64_t (*disable_event)(event_t* ev);

    int64_t (*add_conn)(connection_t* conn);
    int64_t (*del_conn)(connection_t* conn);

    int64_t (*add_udp_sock)(udp_socket_t* sock);
    int64_t (*del_udp_sock)(udp_socket_t* sock);

    int64_t (*process_events)();
};

extern ev_backend_t* ev_backend;
