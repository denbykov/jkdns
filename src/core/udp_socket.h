#pragma once

#include "core/time.h"
#include "decl.h"
#include "buffer.h"
#include "htt.h"
#include "udp_wq.h"

struct udp_socket_s {
    int64_t fd;

    event_t *ev;

    uint32_t bound:1;
    uint32_t non_blocking:1;
    uint32_t error:1;

    uint32_t readable:1;
    uint32_t writable:1;

    connection_ht_t *connections;

    buffer_t last_read_buf;
    udp_wq_t *wq;

    jk_timer_t *timer;
};

udp_socket_t* make_udp_socket();
udp_socket_t* make_client_udp_socket();
void release_udp_socket(udp_socket_t* l);

// ToDo: think about source port randomization for DNS resolver queries
