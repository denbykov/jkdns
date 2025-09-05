#pragma once

#include <stdlib.h>

#include "decl.h"

ssize_t recv_buf(connection_t *conn, buffer_t* buf);
ssize_t send_buf(connection_t *conn, buffer_t* buf);
