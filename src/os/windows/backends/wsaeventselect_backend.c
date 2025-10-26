#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/connection.h"
#include "core/decl.h"
#include "core/errors.h"
#include "core/event.h"
#include "core/ev_backend.h"
#include "core/listener.h"
#include "core/udp_socket.h"
#include "logger/logger.h"
#include "udp_socket/udp_socket.h"

#include "os/windows/winsocket.h"

typedef struct
{
    int64_t  count;
    WSAEVENT wsaEvents[WSAEVENTSELECT_MAX_EVENTS];
    event_t* events[WSAEVENTSELECT_MAX_EVENTS];
} connections_information_t;

static connections_information_t* connectionsInformation;


static int64_t wsaeventselect_init();
static int64_t wsaeventselect_shutdown();
static int64_t wsaeventselect_add_event(event_t* p_eventStruct);
static int64_t wsaeventselect_del_event(event_t* p_eventStruct);
static int64_t wsaeventselect_enable_event(event_t* p_eventStruct);
static int64_t wsaeventselect_disable_event(event_t* p_eventStruct);
static int64_t wsaeventselect_add_conn(connection_t* p_connectionStruct);
static int64_t wsaeventselect_del_conn(connection_t* p_connectionStruct);
static int64_t wsaeventselect_add_udp_sock(udp_socket_t* p_socketStruct);
static int64_t wsaeventselect_del_udp_sock(udp_socket_t* p_socketStruct);
static int64_t wsaeventselect_process_events();
static int64_t GetEventIndex(SOCKET socketDescriptor);


ev_backend_t wsaeventselect_backend = {
    .name           = "wsaeventselect",
    .init           = wsaeventselect_init,
    .shutdown       = wsaeventselect_shutdown,
    .add_event      = wsaeventselect_add_event,
    .del_event      = wsaeventselect_del_event,
    .enable_event   = wsaeventselect_enable_event,
    .disable_event  = wsaeventselect_disable_event,
    .add_conn       = wsaeventselect_add_conn,
    .del_conn       = wsaeventselect_del_conn,
    .process_events = wsaeventselect_process_events
};

static int64_t wsaeventselect_init()
{
    connectionsInformation = calloc(1, sizeof(connections_information_t));

    if (connectionsInformation == NULL)
    {
        return JK_ERROR;
    }

    if (WinsockInit() == JK_ERROR)
    {
        return JK_ERROR;
    }


    return JK_OK;
}

static int64_t wsaeventselect_shutdown()
{
    free(connectionsInformation);


    if (WinsockCleanup() == JK_ERROR)
    {
        return JK_ERROR;
    }


    return JK_OK;
}

static int64_t wsaeventselect_tcp_add_event(
    event_t* p_eventStruct,
    SOCKET   socketDescriptor,
    long     networkEventsToRegister
)
{
    logger_t* logger = current_logger;

    int64_t  eventIndex = 0;
    WSAEVENT wsaEvent;

    eventIndex = GetEventIndex(socketDescriptor);
    if (eventIndex == JK_ERROR)
    {
        wsaEvent = WSACreateEvent();

        if (wsaEvent == WSA_INVALID_EVENT)
        {
            log_perror(
                "wsaeventselect_tcp_add_event: Event creation for socket "
                "failed with error %d",
                WSAGetLastError()
            );


            return JK_ERROR;
        }


        connectionsInformation->wsaEvents[connectionsInformation->count] =
            wsaEvent;

        connectionsInformation->events[connectionsInformation->count] =
            p_eventStruct;

        connectionsInformation->count++;
    }
    else
    {
        wsaEvent =
            connectionsInformation->wsaEvents[connectionsInformation->count];
    }


    int64_t registerEventResult =
        WSAEventSelect(socketDescriptor, wsaEvent, networkEventsToRegister);

    if (registerEventResult == SOCKET_ERROR)
    {
        log_perror(
            "wsaeventselect_tcp_add_event: Registering new event failed with "
            "error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info("wsaeventselect_tcp_add_event: New event was registered!");
    }


    p_eventStruct->enabled = true;


    connectionsInformation->events[connectionsInformation->count - 1] =
        p_eventStruct;


    return JK_OK;
}

static int64_t wsaeventselect_add_event(event_t* p_eventStruct)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor        = INVALID_SOCKET;
    long   networkEventsToRegister = 0;


    if (connectionsInformation->count >= WSAEVENTSELECT_MAX_EVENTS)
    {
        log_perror("wsaeventselect_add_event: No free event slots available");


        return JK_ERROR;
    };


    switch (p_eventStruct->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)p_eventStruct->owner.ptr)->fd;

            networkEventsToRegister = FD_ACCEPT | FD_CLOSE;


            return wsaeventselect_tcp_add_event(
                p_eventStruct,
                socketDescriptor,
                networkEventsToRegister
            );
        case EV_OWNER_CONNECTION:
            connection_t* p_currentConnection = p_eventStruct->owner.ptr;

            if (p_currentConnection->handle.type == CONN_TYPE_TCP)
            {
                socketDescriptor = p_currentConnection->handle.data.fd;


                if ((p_eventStruct->write == true)
                    && (((win_connection_t*)p_currentConnection)->writable
                        == true))
                {
                    p_eventStruct->handler(p_eventStruct);
                }
                if (p_eventStruct->write == true)
                {
                    networkEventsToRegister = FD_WRITE | FD_CLOSE;
                }
                else
                {
                    networkEventsToRegister = FD_READ | FD_CLOSE;
                }


                return wsaeventselect_tcp_add_event(
                    p_eventStruct,
                    socketDescriptor,
                    networkEventsToRegister
                );
            }
            else if (p_currentConnection->handle.type == CONN_TYPE_UDP)
            {
                return udp_add_event(p_eventStruct, p_currentConnection);
            }
            else
            {
                PANIC("bad connection type");
            }


            break;
        default:
            PANIC("unknown event owner");
    }


    return JK_OK;
}

static int64_t wsaeventselect_tcp_del_event(
    event_t* p_eventStruct,
    SOCKET   socketDescriptor
)
{
    logger_t* logger = current_logger;

    int64_t eventIndex = GetEventIndex(socketDescriptor);
    if (eventIndex == JK_ERROR)
    {
        log_perror("wsaeventselect_tcp_del_event: Couldn't find socket index");
        return JK_ERROR;
    }


    if (connectionsInformation->wsaEvents[eventIndex] != NULL)
    {
        if (WSACloseEvent(connectionsInformation->wsaEvents[eventIndex])
            == TRUE)
        {
            log_info(
                "wsaeventselect_tcp_del_event: WSACloseEvent() was successful!"
            );
        }
        else
        {
            log_perror("wsaeventselect_tcp_del_event: WSACloseEvent() failed!");
        }
    }


    for (int64_t elementIndex = eventIndex;
         elementIndex < connectionsInformation->count;
         elementIndex++)
    {
        if (elementIndex == (connectionsInformation->count - 1))
        {
            connectionsInformation->wsaEvents[elementIndex] = NULL;

            connectionsInformation->events[elementIndex] = NULL;
        }
        else
        {
            connectionsInformation->wsaEvents[elementIndex] =
                connectionsInformation->wsaEvents[elementIndex + 1];

            connectionsInformation->events[elementIndex] =
                connectionsInformation->events[elementIndex + 1];
        }
    }


    connectionsInformation->count--;


    p_eventStruct->enabled = false;


    connectionsInformation->events[eventIndex] = p_eventStruct;


    return JK_OK;
}

static int64_t wsaeventselect_del_event(event_t* p_eventStruct)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = INVALID_SOCKET;


    CHECK_INVARIANT(p_eventStruct->owner.ptr != NULL, "event owner is NULL");


    switch (p_eventStruct->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)p_eventStruct->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            connection_t* p_currentConnection = p_eventStruct->owner.ptr;


            if (p_currentConnection->handle.type == CONN_TYPE_TCP)
            {
                socketDescriptor = p_currentConnection->handle.data.fd;


                return wsaeventselect_tcp_del_event(
                    p_eventStruct,
                    socketDescriptor
                );
            }
            else if (p_currentConnection->handle.type == CONN_TYPE_UDP)
            {
                return udp_del_event(p_eventStruct, p_currentConnection);
            }
            else
            {
                PANIC("bad connection type");
            }


            break;
        default:
            PANIC("unknown event owner");
    }


    return JK_OK;
}

static int64_t wsaeventselect_tcp_enable_event(
    event_t* p_eventStruct,
    SOCKET   socketDescriptor,
    long     networkEventsToRegister
)
{
    logger_t* logger = current_logger;

    int64_t eventIndex = GetEventIndex(socketDescriptor);
    if (eventIndex == JK_ERROR)
    {
        log_perror(
            "wsaeventselect_tcp_enable_event: Couldn't find socket index"
        );
        return JK_ERROR;
    }


    int64_t registerEventResult = WSAEventSelect(
        socketDescriptor,
        connectionsInformation->wsaEvents[eventIndex],
        networkEventsToRegister
    );

    if (registerEventResult == SOCKET_ERROR)
    {
        log_perror(
            "wsaeventselect_tcp_enable_event: Registering new event failed "
            "with error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info("wsaeventselect_tcp_enable_event: New event was registered!");
    }


    p_eventStruct->enabled = true;


    connectionsInformation->events[eventIndex] = p_eventStruct;


    return JK_OK;
}

static int64_t wsaeventselect_enable_event(event_t* p_eventStruct)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor        = INVALID_SOCKET;
    long   networkEventsToRegister = 0;


    CHECK_INVARIANT(
        p_eventStruct->enabled == false,
        "event is already enabled"
    );


    CHECK_INVARIANT(p_eventStruct->owner.ptr != NULL, "event owner is NULL");


    switch (p_eventStruct->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)p_eventStruct->owner.ptr)->fd;

            networkEventsToRegister = FD_ACCEPT | FD_CLOSE;


            return wsaeventselect_tcp_enable_event(
                p_eventStruct,
                socketDescriptor,
                networkEventsToRegister
            );
        case EV_OWNER_CONNECTION:
            connection_t* p_currentConnection = p_eventStruct->owner.ptr;


            if (p_currentConnection->handle.type == CONN_TYPE_TCP)
            {
                socketDescriptor = p_currentConnection->handle.data.fd;

                if ((p_eventStruct->write == true)
                    && (((win_connection_t*)p_currentConnection)->writable
                        == true))
                {
                    p_eventStruct->enabled = true;


                    p_eventStruct->handler(p_eventStruct);


                    return JK_OK;
                }
                else if (p_eventStruct->write == true)
                {
                    networkEventsToRegister = FD_WRITE | FD_CLOSE;
                }
                else
                {
                    networkEventsToRegister = FD_READ | FD_CLOSE;
                }


                return wsaeventselect_tcp_enable_event(
                    p_eventStruct,
                    socketDescriptor,
                    networkEventsToRegister
                );
            }
            else if (p_currentConnection->handle.type == CONN_TYPE_UDP)
            {
                return udp_enable_event(p_eventStruct, p_currentConnection);
            }
            else
            {
                PANIC("bad connection type");
            }


            break;
        default:
            PANIC("unknown event owner");
    }


    return JK_OK;
}

static int64_t wsaeventselect_tcp_disable_event(
    event_t* p_eventStruct,
    SOCKET   socketDescriptor
)
{
    logger_t* logger = current_logger;

    int64_t eventIndex = GetEventIndex(socketDescriptor);
    if (eventIndex == JK_ERROR)
    {
        log_perror(
            "wsaeventselect_tcp_disable_event: Couldn't find socket index"
        );
        return JK_ERROR;
    }


    p_eventStruct->enabled = false;


    connectionsInformation->events[eventIndex] = p_eventStruct;


    return JK_OK;
}

static int64_t wsaeventselect_disable_event(event_t* p_eventStruct)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = INVALID_SOCKET;


    CHECK_INVARIANT(
        p_eventStruct->enabled == true,
        "event is already disabled"
    );


    CHECK_INVARIANT(p_eventStruct->owner.ptr != NULL, "event owner is NULL");


    switch (p_eventStruct->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)p_eventStruct->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            connection_t* p_currentConnection = p_eventStruct->owner.ptr;


            if (p_currentConnection->handle.type == CONN_TYPE_TCP)
            {
                socketDescriptor = p_currentConnection->handle.data.fd;


                return wsaeventselect_tcp_disable_event(
                    p_eventStruct,
                    socketDescriptor
                );
            }
            else if (p_currentConnection->handle.type == CONN_TYPE_UDP)
            {
                return udp_disable_event(p_eventStruct, p_currentConnection);
            }
            else
            {
                PANIC("bad connection type");
            }


            break;
        default:
            PANIC("unknown event owner");
    }


    return JK_OK;
}

static int64_t wsaeventselect_add_conn(connection_t* p_connectionStruct)
{
    logger_t* logger = current_logger;

    int64_t eventIndex              = 0;
    SOCKET  socketDescriptor        = INVALID_SOCKET;

    WSAEVENT wsaEvent;


    if (connectionsInformation->count >= WSAEVENTSELECT_MAX_EVENTS)
    {
        log_perror("wsaeventselect_add_conn: No free event slots available");


        return JK_ERROR;
    };


    if (p_connectionStruct->handle.type == CONN_TYPE_TCP)
    {
        socketDescriptor = p_connectionStruct->handle.data.fd;
    }
    else if (p_connectionStruct->handle.type == CONN_TYPE_UDP)
    {
        PANIC("unimplemented");
    }
    else
    {
        PANIC("bad connection type");
    }


    eventIndex = GetEventIndex(socketDescriptor);
    if (eventIndex == JK_ERROR)
    {
        wsaEvent = WSACreateEvent();

        if (wsaEvent == WSA_INVALID_EVENT)
        {
            log_perror(
                "wsaeventselect_add_event: Event creation for socket failed "
                "with "
                "error %d",
                WSAGetLastError()
            );


            return JK_ERROR;
        }


        connectionsInformation->wsaEvents[connectionsInformation->count] =
            wsaEvent;

        connectionsInformation->events[connectionsInformation->count] =
            p_connectionStruct->write;

        connectionsInformation->count++;
    }
    else
    {
        wsaEvent =
            connectionsInformation->wsaEvents[connectionsInformation->count];
    }


    return JK_OK;
}

static int64_t wsaeventselect_del_conn(connection_t* p_connectionStruct)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = INVALID_SOCKET;


    if (p_connectionStruct->handle.type == CONN_TYPE_TCP)
    {
        socketDescriptor = p_connectionStruct->handle.data.fd;
    }
    else if (p_connectionStruct->handle.type == CONN_TYPE_UDP)
    {
        PANIC("unimplemented");
    }
    else
    {
        PANIC("bad connection type");
    }


    int64_t eventIndex = GetEventIndex(socketDescriptor);

    if (eventIndex == JK_ERROR)
    {
        log_perror("wsaeventselect_del_conn: Couldn't find socket index");
        return JK_ERROR;
    }


    if (connectionsInformation->wsaEvents[eventIndex] != NULL)
    {
        if (WSACloseEvent(connectionsInformation->wsaEvents[eventIndex])
            == TRUE)
        {
            log_info(
                "wsaeventselect_del_conn: WSACloseEvent() was successful!"
            );
        }
        else
        {
            log_perror("wsaeventselect_del_conn: WSACloseEvent() failed!");
        }
    }


    for (int64_t elementIndex = eventIndex;
         elementIndex < connectionsInformation->count;
         elementIndex++)
    {
        if (elementIndex == (connectionsInformation->count - 1))
        {
            connectionsInformation->wsaEvents[elementIndex] = NULL;

            connectionsInformation->events[elementIndex] = NULL;
        }
        else
        {
            connectionsInformation->wsaEvents[elementIndex] =
                connectionsInformation->wsaEvents[elementIndex + 1];

            connectionsInformation->events[elementIndex] =
                connectionsInformation->events[elementIndex + 1];
        }
    }


    connectionsInformation->count--;


    return JK_OK;
}

static int64_t wsaeventselect_process_events()
{
    logger_t* logger = current_logger;


    SOCKET socketDescriptor = INVALID_SOCKET;


    WSANETWORKEVENTS networkEvents;


    int eventIndex = WSAWaitForMultipleEvents(
        (DWORD)(connectionsInformation->count),
        connectionsInformation->wsaEvents,
        FALSE,
        WSA_INFINITE,
        FALSE
    );

    if (eventIndex == WSA_WAIT_FAILED)
    {
        log_perror(
            "wsaeventselect_process_events: WSAWaitForMultipleEvents() failed "
            "with error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        eventIndex -= WSA_WAIT_EVENT_0;


        log_info(
            "wsaeventselect_process_events: Socket %d signalled back!",
            eventIndex
        );
    }


    event_t* p_currentEvent = connectionsInformation->events[eventIndex];

    switch (p_currentEvent->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)p_currentEvent->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            connection_t* p_currentConnection = p_currentEvent->owner.ptr;
            socketDescriptor = p_currentConnection->handle.data.fd;
            break;
        case EV_OWNER_USOCK:
            udp_socket_t* p_currentUdpSocket = p_currentEvent->owner.ptr;
            socketDescriptor                 = p_currentUdpSocket->fd;
            break;
        default:
            PANIC("unknown event owner");
    }


    int wsaEnumNetworkEventsResult = WSAEnumNetworkEvents(
        socketDescriptor,
        connectionsInformation->wsaEvents[eventIndex],
        &networkEvents
    );

    if (wsaEnumNetworkEventsResult == SOCKET_ERROR)
    {
        log_perror(
            "wsaeventselect_process_events: WSAEnumNetworkEvents() failed with "
            "error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info(
            "wsaeventselect_process_events: Enumerated network events for "
            "socket %d!",
            eventIndex
        );
    }


    if ((networkEvents.lNetworkEvents & FD_ACCEPT)
        && (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0))
    {
        log_trace(
            "wsaeventselect_process_events: FD_ACCEPT event failure detected"
        );


        errno = networkEvents.iErrorCode[FD_ACCEPT_BIT];
    }
    else if ((networkEvents.lNetworkEvents & FD_READ)
             && (networkEvents.iErrorCode[FD_READ_BIT] != 0))
    {
        log_trace(
            "wsaeventselect_process_events: FD_READ event failure detected"
        );


        errno = networkEvents.iErrorCode[FD_READ_BIT];
    }
    else if ((networkEvents.lNetworkEvents & FD_WRITE)
             && (networkEvents.iErrorCode[FD_WRITE_BIT] != 0))
    {
        log_trace(
            "wsaeventselect_process_events: FD_WRITE event failure detected"
        );


        errno = networkEvents.iErrorCode[FD_WRITE_BIT];
    }
    else if ((networkEvents.lNetworkEvents & FD_READ)
             && (p_currentEvent->owner.tag == EV_OWNER_USOCK))
    {
        udp_socket_t* p_currentUdpSocket = p_currentEvent->owner.ptr;
        p_currentUdpSocket->readable     = true;
    }
    else if ((networkEvents.lNetworkEvents & FD_WRITE)
             && (p_currentEvent->owner.tag == EV_OWNER_USOCK))
    {
        udp_socket_t* p_currentUdpSocket = p_currentEvent->owner.ptr;
        p_currentUdpSocket->writable     = true;
    }


    CHECK_INVARIANT(p_currentEvent->handler != NULL, "event handler is NULL");


    p_currentEvent->handler(p_currentEvent);


    return JK_OK;
}

static int64_t GetEventIndex(SOCKET socketDescriptor)
{
    for (int64_t eventIndex = 0; eventIndex < connectionsInformation->count;
         eventIndex++)
    {
        win_connection_t* currentConnection =
            (connectionsInformation->events[eventIndex])->owner.ptr;

        if ((SOCKET)currentConnection->connection.handle.data.fd
            == socketDescriptor)
        {
            return eventIndex;
        }
    }


    return JK_ERROR;
}

static int64_t wsaeventselect_add_udp_sock(udp_socket_t* p_socketStruct)
{
    p_socketStruct = NULL;
    return JK_OK;
}

static int64_t wsaeventselect_del_udp_sock(udp_socket_t* p_socketStruct)
{
    p_socketStruct = NULL;
    return JK_OK;
}
