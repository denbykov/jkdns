#pragma once

#include "decl.h"
#include "io.h"

struct connection_s {
    event_t *read;
    event_t *write;
    recv_pt recv;
    send_pt send;
};
