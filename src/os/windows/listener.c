#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core/decl.h"
#include "core/listener.h"
#include "core/event.h"
#include "connection/connection.h"
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


    settings_t* s      = current_settings;
    logger_t*   logger = current_logger;


    listener_t* l = calloc(1, sizeof(listener_t));

    if (l == NULL)
    {
        log_perror("make_listener.allocate_event_list");
        return NULL;
    }


    l->accept = NULL;
    l->fd     = -1;


    SOCKET fd = INVALID_SOCKET;


    SOCKADDR_IN server_sockaddr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(s->port),
        .sin_addr.s_addr = INADDR_ANY
    };

    memset(&(server_sockaddr.sin_zero), 0, 8);


    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == INVALID_SOCKET)
    {
        log_perror("make_listener.socket");
        l->error = true;
        return l;
    }


    /*
    bool opt = true;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        log_perror("make_listener.setsockopt");


        closesocket(fd);
        l->error = true;
        return l;
    }
    */


    if (bind(fd, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr))
        != 0)
    {
        log_perror("make_listener.bind");


        closesocket(fd);
        l->error = true;
        return l;
    }


    l->fd    = fd;
    l->bound = true;


    if (listen(fd, LISTEN_QUEUE) != 0)
    {
        log_perror("make_listener.listen");
        l->error = true;
        return l;
    }


    l->listening = true;


    l->non_blocking = true;


    return l;
}

void release_listener(listener_t* l)
{
    if (l != NULL)
    {
        free(l);
    }


    WinsockCleanup();
}

void accept_handler(event_t* ev)
{
    SOCKET   socketDescriptor = INVALID_SOCKET;
    SOCKADDR remoteAddress;
    int      remoteAddressLength = sizeof(remoteAddress);

    logger_t* logger = current_logger;


    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");


    switch (ev->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)ev->owner.ptr)->fd; // NOLINT
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

    handle_new_connection(connectionSocket);
}
