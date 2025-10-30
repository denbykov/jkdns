#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "connection/connection.h"
#include "core/decl.h"
#include "core/event.h"
#include "core/listener.h"
#include "logger/logger.h"
#include "settings/settings.h"

#include "os/windows/winsocket.h"


#define LISTEN_QUEUE 10


listener_t* make_listener()
{
    if (WinsockInit() != JK_OK)
    {
        return NULL;
    }


    settings_t* p_currentSettings = current_settings;
    logger_t*   logger            = current_logger;

    struct sockaddr_in serverSockaddr  = {0};
    SOCKET             listeningSocket = INVALID_SOCKET;
    BOOL               optVal          = true;


    listener_t* p_listenerStruct = calloc(1, sizeof(listener_t));
    if (p_listenerStruct == NULL)
    {
        log_perror("make_listener.allocate_event_list");
        return NULL;
    }


    p_listenerStruct->accept = NULL;
    p_listenerStruct->fd     = INVALID_SOCKET;


    serverSockaddr.sin_family      = AF_INET;
    serverSockaddr.sin_port        = htons(p_currentSettings->port);
    serverSockaddr.sin_addr.s_addr = INADDR_ANY;


    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET)
    {
        log_perror("make_listener.socket");
        p_listenerStruct->error = true;
        return p_listenerStruct;
    }


    if (setsockopt(
            listeningSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &optVal,
            sizeof(optVal)
        ))
    {
        log_perror("make_listener.setsockopt");


        closesocket(listeningSocket);


        p_listenerStruct->error = true;


        return p_listenerStruct;
    }


    if (bind(
            listeningSocket,
            (struct sockaddr*)&serverSockaddr,
            sizeof(serverSockaddr)
        )
        != 0)
    {
        log_perror("make_listener.bind");


        closesocket(listeningSocket);


        p_listenerStruct->error = true;


        return p_listenerStruct;
    }


    p_listenerStruct->fd    = listeningSocket;
    p_listenerStruct->bound = true;


    if (listen(listeningSocket, LISTEN_QUEUE) != 0)
    {
        log_perror("make_listener.listen");
        p_listenerStruct->error = true;
        return p_listenerStruct;
    }


    p_listenerStruct->listening    = true;
    p_listenerStruct->non_blocking = true;
    return p_listenerStruct;
}

void release_listener(listener_t* l)
{
    if (l != NULL)
    {
        free(l);
    }


    WinsockCleanup();
}

void accept_handler(event_t* p_eventStruct)
{
    logger_t* logger = current_logger;

    SOCKET   socketDescriptor = INVALID_SOCKET;
    SOCKADDR remoteAddress;
    int      remoteAddressLength = sizeof(remoteAddress);


    CHECK_INVARIANT(p_eventStruct->owner.ptr != NULL, "event owner is NULL");


    switch (p_eventStruct->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor =
                ((listener_t*)p_eventStruct->owner.ptr)->fd; // NOLINT
            break;
        default:
            PANIC("unexpected event owner");
    }


    SOCKET connectionSocket =
        accept(socketDescriptor, &remoteAddress, &remoteAddressLength);
    if (connectionSocket == INVALID_SOCKET)
    {
        log_perror(
            "accept_handler.accept: Connection accept was unsuccessfull. "
            "Error code: %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info("accept_handler.accept: Connection accept was successfull.");


        // SocketResolveAddress(&remoteAddress, remoteAddressLength);
    }


    handle_new_tcp_connection(connectionSocket);
}
