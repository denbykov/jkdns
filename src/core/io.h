#pragma once

#include <stdlib.h>

#include "decl.h"

ssize_t recv_buf(connection_t *conn, uint8_t* buf, size_t count);
ssize_t send_buf(connection_t *conn, uint8_t* buf, size_t count);
