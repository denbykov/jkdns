#pragma once

#include "core/decl.h"
#include <stdint.h>

#include "core/connection.h"

void handle_new_tcp_connection(int64_t fd);
// connection_t *tcp_connect(const char* ip, uint16_t port);
void close_connection(connection_t* conn);
