#pragma once

#include "decl.h"

#include <netinet/in.h> // for in_addr/in6_addr, which should be binary compatible with win
#include <stdint.h>

struct address_s {
    uint8_t af;
    uint16_t src_port;
    union {
        struct in_addr  src_v4;
        struct in6_addr src_v6;
    } src;
};

typedef enum {
    CONN_TYPE_TCP,
    CONN_TYPE_UDP,
} conn_type_t;

typedef struct {
    conn_type_t type;
    union {
        int64_t fd;
        udp_socket_t *sock;
    } data;
} conn_handle_t;

struct connection_s {
    void *data;

    address_t address;
    conn_handle_t handle;

    event_t *read;
    event_t *write;

    uint32_t error:1;

    // recv_pt recv;
    // send_pt send;
};
