#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "decl.h"

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count);
ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count);

int64_t open_tcp_conn(const char* ip, uint16_t port);
void close_tcp_conn(int64_t fd);
