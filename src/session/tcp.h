#pragma once

#include <stdint.h>

#include <core/connection.h>

void handle_new_connection(int64_t fd);
void close_connection(connection_t* conn);
