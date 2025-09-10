#pragma once

#include "decl.h"

struct connection_s {
    void *data;

    // ToDo: replace with the structure that has fd 
    // and some other connection information like client address?
    // In any case add address here somehow
    int64_t fd;

    event_t *read;
    event_t *write;

    // ToDo: handle connection establishment correctly
    uint32_t error:1;

    // recv_pt recv;
    // send_pt send;
};
