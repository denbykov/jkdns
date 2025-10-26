#pragma once

#include "core/decl.h"
#include "core/udp_socket.h"

#include <stdint.h>

void udp_ev_handler(event_t* ev);
int64_t udp_add_event(event_t* ev, connection_t* conn);
int64_t udp_del_event(event_t* ev, connection_t* conn);
int64_t udp_enable_event(event_t* ev, connection_t* conn);
int64_t udp_disable_event(event_t* ev, connection_t* conn);
