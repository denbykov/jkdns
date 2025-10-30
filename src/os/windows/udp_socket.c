#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/decl.h"
#include "core/ht.h"
#include "core/htt.h"
#include "core/udp_socket.h"
#include "core/udp_wq.h"
#include "logger/logger.h"
#include "settings/settings.h"

#include "os/windows/winsocket.h"


#define LISTEN_QUEUE 10


udp_socket_t* make_udp_socket()
{
    if (WinsockInit() != JK_OK)
    {
        return NULL;
    }


    settings_t* p_currentSettings = current_settings;
    logger_t*   logger            = current_logger;

    struct sockaddr_in serverSockaddr = {0};
    SOCKET             serverSocket   = INVALID_SOCKET;
    BOOL               optVal         = true;
    buffer_t           udpBuffer      = {0};


    udp_socket_t* p_socketStruct = calloc(1, sizeof(udp_socket_t));
    if (p_socketStruct == NULL)
    {
        log_perror("make_udp_socket.allocate_event_list");
        return NULL;
    }


    p_socketStruct->connections = connection_ht_create(128);
    if (p_socketStruct->connections == NULL)
    {
        log_perror("make_udp_socket.allocate_connections_ht");
        p_socketStruct->error = true;
        return p_socketStruct;
    }


    p_socketStruct->wq = udp_wq_create(128);
    if (p_socketStruct->wq == NULL)
    {
        log_perror("make_udp_socket.allocate_write_queue");
        p_socketStruct->error = true;
        return p_socketStruct;
    }


    p_socketStruct->ev       = NULL;
    p_socketStruct->fd       = -1;
    p_socketStruct->readable = false;
    p_socketStruct->writable = false;


    serverSockaddr.sin_family      = AF_INET;
    serverSockaddr.sin_port        = htons(p_currentSettings->port);
    serverSockaddr.sin_addr.s_addr = INADDR_ANY;


    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        log_perror("make_udp_socket.socket");
        p_socketStruct->error = true;
        return p_socketStruct;
    }


    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &optVal,
            sizeof(optVal)
        )
        == SOCKET_ERROR)
    {
        log_perror("make_udp_socket.setsockopt");
        p_socketStruct->error = true;
        return p_socketStruct;
    }


    if (bind(
            serverSocket,
            (struct sockaddr*)&serverSockaddr,
            sizeof(struct sockaddr)
        )
        == SOCKET_ERROR)
    {
        log_perror("make_udp_socket.bind");


        p_socketStruct->error = true;


        closesocket(serverSocket);


        return p_socketStruct;
    }


    p_socketStruct->fd    = serverSocket;
    p_socketStruct->bound = true;


    udpBuffer.data = calloc(UDP_MSG_SIZE, sizeof(*udpBuffer.data));
    if (udpBuffer.data == NULL)
    {
        log_perror("make_udp_socket.allocate_buffer");


        p_socketStruct->error = true;


        closesocket(serverSocket);


        return p_socketStruct;
    }


    udpBuffer.capacity = UDP_MSG_SIZE;
    udpBuffer.taken    = 0;


    p_socketStruct->last_read_buf = udpBuffer;


    p_socketStruct->non_blocking = true;


    return p_socketStruct;
}

void release_udp_socket(udp_socket_t* p_socketStruct)
{
    logger_t* logger = current_logger;


    CHECK_INVARIANT(p_socketStruct != NULL, "sock is NULL");


    if (p_socketStruct->last_read_buf.data != NULL)
    {
        free(p_socketStruct->last_read_buf.data);
    }


    closesocket(p_socketStruct->fd);


    if (p_socketStruct->wq != NULL)
    {
        udp_wq_destroy(p_socketStruct->wq);
    }


    if (p_socketStruct->connections != NULL)
    {
        connection_ht_destroy(p_socketStruct->connections);
    }


    free(p_socketStruct);


    WinsockCleanup();
}
