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
#include "logger/logger.h"

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
static int64_t wsaeventselect_add_event(event_t* ev);
static int64_t wsaeventselect_del_event(event_t* ev);
static int64_t wsaeventselect_enable_event(event_t* ev);
static int64_t wsaeventselect_disable_event(event_t* ev);
static int64_t wsaeventselect_add_conn(connection_t* conn);
static int64_t wsaeventselect_del_conn(connection_t* conn);
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

static int64_t wsaeventselect_add_event(event_t* ev)
{
    logger_t* logger = current_logger;

    int64_t eventIndex              = 0;
    SOCKET  socketDescriptor        = INVALID_SOCKET;
    long    networkEventsToRegister = 0;

    WSAEVENT wsaEvent;


    if (connectionsInformation->count >= WSAEVENTSELECT_MAX_EVENTS)
    {
        log_perror("wsaeventselect_add_event: No free event slots available");


        return JK_ERROR;
    };


    switch (ev->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)ev->owner.ptr)->fd;

            networkEventsToRegister = FD_ACCEPT | FD_CLOSE;

            break;
        case EV_OWNER_CONNECTION:
            socketDescriptor = ((connection_t*)ev->owner.ptr)->fd;

            win_connection_t* current_connection = ev->owner.ptr;
            if (ev->write && current_connection->writable)
            {
                ev->handler(ev);
            }
            // store previous flags in a unified array of
            // sockets/events/connections
            if (ev->write)
            {
                networkEventsToRegister = FD_WRITE | FD_CLOSE;
            }
            else
            {
                networkEventsToRegister = FD_READ | FD_CLOSE;
            }

            break;
        default:
            PANIC("unknown event owner");
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

        connectionsInformation->events[connectionsInformation->count] = ev;

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
            "wsaeventselect_add_event: Registering new event failed with "
            "error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info("wsaeventselect_add_event: New event was registered!");
    }


    ev->enabled = true;


    connectionsInformation->events[connectionsInformation->count - 1] = ev;


    return JK_OK;
}

static int64_t wsaeventselect_del_event(event_t* ev)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = INVALID_SOCKET;


    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");


    switch (ev->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            socketDescriptor = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            PANIC("unknown event owner");
    }


    int64_t eventIndex = GetEventIndex(socketDescriptor);

    if (eventIndex == JK_ERROR)
    {
        log_perror("wsaeventselect_del_event: Couldn't find socket index");
        return JK_ERROR;
    }


    if (connectionsInformation->wsaEvents[eventIndex] != NULL)
    {
        if (WSACloseEvent(connectionsInformation->wsaEvents[eventIndex])
            == TRUE)
        {
            log_info(
                "wsaeventselect_del_event: WSACloseEvent() was successful!"
            );
        }
        else
        {
            log_perror("wsaeventselect_del_event: WSACloseEvent() failed!");
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


    ev->enabled = false;


    connectionsInformation->events[eventIndex] = ev;


    return JK_OK;
}

static int64_t wsaeventselect_enable_event(event_t* ev)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor        = INVALID_SOCKET;
    long   networkEventsToRegister = 0;


    CHECK_INVARIANT(ev->enabled == false, "event is already enabled");


    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");


    switch (ev->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)ev->owner.ptr)->fd;

            networkEventsToRegister = FD_ACCEPT | FD_CLOSE;

            break;
        case EV_OWNER_CONNECTION:
            socketDescriptor = ((connection_t*)ev->owner.ptr)->fd;

            if (ev->write && ((win_connection_t*)ev->owner.ptr)->writable)
            {
                ev->enabled = true;


                ev->handler(ev);


                return JK_OK;
            }
            else if (ev->write)
            {
                networkEventsToRegister = FD_WRITE | FD_CLOSE;
            }
            else
            {
                networkEventsToRegister = FD_READ | FD_CLOSE;
            }

            break;
        default:
            PANIC("unknown event owner");
    }


    int64_t eventIndex = GetEventIndex(socketDescriptor);

    if (eventIndex == JK_ERROR)
    {
        log_perror("wsaeventselect_enable_event: Couldn't find socket index");
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
            "wsaeventselect_enable_event: Registering new event failed with "
            "error %d",
            WSAGetLastError()
        );


        return JK_ERROR;
    }
    else
    {
        log_info("wsaeventselect_enable_event: New event was registered!");
    }


    ev->enabled = true;


    connectionsInformation->events[eventIndex] = ev;


    return JK_OK;
}

static int64_t wsaeventselect_disable_event(event_t* ev)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = INVALID_SOCKET;


    CHECK_INVARIANT(ev->enabled == true, "event is already disabled");


    CHECK_INVARIANT(ev->owner.ptr != NULL, "event owner is NULL");


    switch (ev->owner.tag)
    {
        case EV_OWNER_LISTENER:
            socketDescriptor = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            socketDescriptor = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            PANIC("unknown event owner");
    }


    int64_t eventIndex = GetEventIndex(socketDescriptor);

    if (eventIndex == JK_ERROR)
    {
        log_perror("wsaeventselect_disable_event: Couldn't find socket index");
        return JK_ERROR;
    }


    ev->enabled = false;


    connectionsInformation->events[eventIndex] = ev;


    return JK_OK;
}

static int64_t wsaeventselect_add_conn(connection_t* conn)
{
    logger_t* logger = current_logger;

    int64_t eventIndex              = 0;
    SOCKET  socketDescriptor        = INVALID_SOCKET;
    long    networkEventsToRegister = 0;

    WSAEVENT wsaEvent;


    if (connectionsInformation->count >= WSAEVENTSELECT_MAX_EVENTS)
    {
        log_perror("wsaeventselect_add_conn: No free event slots available");


        return JK_ERROR;
    };


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
            conn->write;

        connectionsInformation->count++;
    }
    else
    {
        wsaEvent =
            connectionsInformation->wsaEvents[connectionsInformation->count];
    }


    return JK_OK;
}

static int64_t wsaeventselect_del_conn(connection_t* conn)
{
    logger_t* logger = current_logger;

    SOCKET socketDescriptor = conn->fd;


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
            socketDescriptor = ((connection_t*)p_currentEvent->owner.ptr)->fd;
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

        if (currentConnection->connection.fd == socketDescriptor)
        {
            return eventIndex;
        }
    }


    return JK_ERROR;
}
