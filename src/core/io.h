#pragma once

#include <stdlib.h>

#include "decl.h"

typedef ssize_t (*recv_pt)(connection_t *c, u_char *buf, size_t size);
typedef ssize_t (*send_pt)(connection_t *c, u_char *buf, size_t size);
 