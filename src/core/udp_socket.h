#pragma once

#include "decl.h"
#include "ht.h"

struct udp_socket_s {
    int64_t fd;

    event_t *read;
    event_t *write;

    uint32_t bound:1;
    uint32_t non_blocking:1;
    uint32_t error:1;

    connection_ht_t connections;
};

udp_socket_t* make_udp_socket();
void release_udp_socket(udp_socket_t* l);
// void accept_handler(event_t* ev);
