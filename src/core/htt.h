#pragma once

#include "ht.h"
#include "decl.h"
#include "connection.h"

typedef address_t connection_key_t;
DECLARE_HT(connection, connection_key_t, connection_t)
DECLARE_HT(udp_wq, connection_key_t, event_t*)
