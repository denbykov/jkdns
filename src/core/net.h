#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "decl.h"

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count);
ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count);

ssize_t udp_recv(udp_socket_t *sock, uint8_t* buf, size_t count, address_t* address);
ssize_t udp_send(udp_socket_t *sock, uint8_t* buf, size_t count, address_t* address);

int64_t open_tcp_conn(address_t* address);
void close_tcp_conn(int64_t fd);
