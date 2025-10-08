#pragma once

#include "core/decl.h"
#include "core/connection.h"

#include <stdint.h>
#include <stdbool.h>

void handle_new_tcp_connection(int64_t fd);
connection_t *make_udp_connection(udp_socket_t* sock, address_t* address);
connection_t *make_client_connection(
    conn_type_t type,
    const char* ip,
    uint16_t port,
    void (*read_handler)(event_t *ev),
    void (*write_handler)(event_t *ev));
void close_connection(connection_t* conn);
int64_t fill_address(struct address_s* addr, const char* ip, uint16_t port);
