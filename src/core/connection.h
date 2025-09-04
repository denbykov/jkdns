#pragma once

#include "decl.h"

struct connection_s {
    uint64_t fd;

    event_t *read;
    event_t *write;
    // recv_pt recv;
    // send_pt send;
};
