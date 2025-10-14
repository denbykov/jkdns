#pragma once

#include "core/connection.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")


#define WSAEVENTSELECT_MAX_EVENTS WSA_MAXIMUM_WAIT_EVENTS


typedef struct
{
    connection_t connection;

    uint8_t writable :1;
} win_connection_t;


int64_t WinsockInit();
int64_t WinsockCleanup();
int64_t GetEventIndex(SOCKET socketDescriptor);
