#pragma once

#include "ht.h"

#include "connection.h"
typedef address_t connection_key_t;
DECLARE_HT(connection, connection_key_t, connection_t)
