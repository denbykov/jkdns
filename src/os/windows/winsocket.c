#include "logger/logger.h"

#include "os/windows/winsocket.h"


connection_t* allocate_connection()
{
    logger_t* logger = current_logger;

    win_connection_t* connection;


    connection = calloc(1, sizeof(win_connection_t));
    if (connection == NULL)
    {
        log_perror("WSA Startup failed with error %d", errno);
        return NULL;
    }


    connection->writable = true;


    return (connection_t*)connection;
}

int64_t WinsockInit()
{
    logger_t* logger = current_logger;

    WORD    wsaVersion = MAKEWORD(2, 2);
    WSADATA wsaData;


    int64_t wsaStartupResult = WSAStartup(wsaVersion, &wsaData);

    if (wsaStartupResult != 0)
    {
        log_perror("WSA Startup failed with error %d", wsaStartupResult);


        return JK_ERROR;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        log_perror("Could not find a usable version of Winsock.dll");


        WSACleanup();
        return JK_ERROR;
    }
    else
    {
        log_info(
            "Winsock.dll of version %d.%d was loaded",
            LOBYTE(wsaData.wVersion),
            HIBYTE(wsaData.wVersion)
        );
    }


    return JK_OK;
}

int64_t WinsockCleanup()
{
    WSACleanup();


    return JK_OK;
}
