#pragma once

#include "decl.h"

#include <netinet/in.h> // for in_addr/in6_addr, which should be binary compatible with win

typedef struct {
    uint8_t af;
    uint16_t src_port;
    union {
        struct in_addr  src_v4;
        struct in6_addr src_v6;
    } src;
} address_t;

struct connection_s {
    void *data;

    address_t address;
    int64_t fd;

    event_t *read;
    event_t *write;

    uint32_t is_udp:1;
    uint32_t error:1;

    // recv_pt recv;
    // send_pt send;
};
