#include <stdint.h>
#include <errno.h>

#include "core/buffer.h"
#include "core/connection.h"
#include "core/decl.h"
#include "core/errors.h"
#include "core/net.h"
#include "core/udp_socket.h"
#include "logger/logger.h"

#include "os/windows/winsocket.h"


ssize_t recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
);
static ssize_t tcp_recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
);
static ssize_t udp_recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
);

ssize_t send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
);
static ssize_t tcp_send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
);
static ssize_t udp_send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
);


ssize_t recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;


    CHECK_INVARIANT(p_connectionStruct != NULL, "conn is null");

    CHECK_INVARIANT(p_recvBuffer != NULL, "buf is null");


    if (p_connectionStruct->handle.type == CONN_TYPE_TCP)
    {
        return tcp_recv_buf(p_connectionStruct, p_recvBuffer, bufferLength);
    }
    else if (p_connectionStruct->handle.type == CONN_TYPE_UDP)
    {
        return udp_recv_buf(p_connectionStruct, p_recvBuffer, bufferLength);
    }
    else
    {
        PANIC("Unexpected connection type");
    }


    return JK_ERROR;
}

ssize_t tcp_recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;

    SOCKET  socketDescriptor   = p_connectionStruct->handle.data.fd;
    size_t  spaceLeft          = bufferLength;
    ssize_t bytesReceivedTotal = 0;


    CHECK_INVARIANT(p_connectionStruct != NULL, "conn is null");

    CHECK_INVARIANT(p_recvBuffer != NULL, "buf is null");


    for (;;)
    {
        if (spaceLeft <= 0)
        {
            log_warn("recv_buf: no space left to read into");
            return JK_OUT_OF_BUFFER;
        }


        ssize_t bytesReceived =
            recv(socketDescriptor, p_recvBuffer, spaceLeft, 0);
        if (bytesReceived == 0)
        {
            break;
        }
        else if ((bytesReceived == SOCKET_ERROR)
                 && (WSAGetLastError() == WSAEWOULDBLOCK))
        {
            break;
        }
        else if (bytesReceived == SOCKET_ERROR)
        {
            return JK_ERROR;
        }


        bytesReceivedTotal += bytesReceived;
        spaceLeft          -= bytesReceived;
        p_recvBuffer       += bytesReceived * sizeof(*p_recvBuffer);
    }

    return bytesReceivedTotal;
}

static ssize_t udp_recv_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;

    udp_socket_t* p_socketStruct = p_connectionStruct->handle.data.sock;


    CHECK_INVARIANT(p_socketStruct != NULL, "sock is NULL");

    CHECK_INVARIANT(
        p_socketStruct->last_read_buf.taken <= bufferLength,
        "cannot copy whole buffer"
    );

    memcpy(
        p_recvBuffer,
        p_socketStruct->last_read_buf.data,
        p_socketStruct->last_read_buf.taken
    );

    return (ssize_t)p_socketStruct->last_read_buf.taken;
}

ssize_t send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;


    CHECK_INVARIANT(p_connectionStruct != NULL, "conn is null");

    CHECK_INVARIANT(p_sendBuffer != NULL, "buf is null");


    if (p_connectionStruct->handle.type == CONN_TYPE_TCP)
    {
        return tcp_send_buf(p_connectionStruct, p_sendBuffer, bufferLength);
    }
    else if (p_connectionStruct->handle.type == CONN_TYPE_UDP)
    {
        return udp_send_buf(p_connectionStruct, p_sendBuffer, bufferLength);
    }
    else
    {
        PANIC("Unexpected connection type");
    }


    return JK_ERROR;
}

static ssize_t tcp_send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;


    CHECK_INVARIANT(p_connectionStruct != NULL, "conn is null");

    CHECK_INVARIANT(p_sendBuffer != NULL, "buf is null");


    SOCKET  socketDescriptor = p_connectionStruct->handle.data.fd;
    size_t  bytesLeft        = bufferLength;
    ssize_t bytesSentTotal   = 0;


    for (;;)
    {
        if (bytesLeft == 0)
        {
            break;
        }


        ssize_t bytesSent = send(socketDescriptor, p_sendBuffer, bytesLeft, 0);
        if (bytesSent == 0)
        {
            break;
        }
        else if ((bytesSent == SOCKET_ERROR)
                 && (WSAGetLastError() == WSAEWOULDBLOCK))
        {
            ((win_connection_t*)p_connectionStruct)->writable = false;
            break;
        }
        else if (bytesSent == SOCKET_ERROR)
        {
            return JK_ERROR;
        }


        bytesSentTotal += bytesSent;
        bytesLeft      -= bytesSent;
        p_sendBuffer   += bytesSent * sizeof(*p_sendBuffer);
    }


    return bytesSentTotal;
}

static ssize_t udp_send_buf(
    connection_t* p_connectionStruct,
    uint8_t*      p_sendBuffer,
    size_t        bufferLength
)
{
    logger_t* logger = current_logger;

    udp_socket_t* p_socketStruct   = p_connectionStruct->handle.data.sock;
    SOCKET        scoketDescriptor = p_socketStruct->fd;
    ssize_t       bytesSent        = 0;


    CHECK_INVARIANT(p_connectionStruct != NULL, "conn is null");

    CHECK_INVARIANT(p_sendBuffer != NULL, "buf is null");

    CHECK_INVARIANT(bufferLength != 0, "count is 0");

    CHECK_INVARIANT(p_socketStruct != NULL, "sock is null");


    if (p_connectionStruct->address.af == AF_INET)
    {
        struct sockaddr_in clientAddres4 = {0};

        clientAddres4.sin_family = AF_INET;
        clientAddres4.sin_port   = htons(p_connectionStruct->address.src_port);
        clientAddres4.sin_addr   = p_connectionStruct->address.src.src_v4;


        bytesSent = sendto(
            scoketDescriptor,
            p_sendBuffer,
            bufferLength,
            0,
            (struct sockaddr*)&clientAddres4,
            sizeof(clientAddres4)
        );
    }
    else if (p_connectionStruct->address.af == AF_INET6)
    {
        struct sockaddr_in6 clientAddres6 = {0};

        clientAddres6.sin6_family = AF_INET6;
        clientAddres6.sin6_port   = htons(p_connectionStruct->address.src_port);
        clientAddres6.sin6_addr   = p_connectionStruct->address.src.src_v6;


        bytesSent = sendto(
            scoketDescriptor,
            p_sendBuffer,
            bufferLength,
            0,
            (struct sockaddr*)&clientAddres6,
            sizeof(clientAddres6)
        );
    }
    else
    {
        PANIC("Unrecognized address family");
    }

    if ((bytesSent == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK))
    {
        p_socketStruct->writable = false;
        return JK_WOULD_BLOCK;
    }

    if (bytesSent == SOCKET_ERROR)
    {
        log_perror("udp_send_buf.sendto");
        return JK_ERROR;
    }


    return bytesSent;
}

int64_t open_tcp_conn(const char* serverIpAddress, uint16_t serverPort)
{
    logger_t* logger = current_logger;

    SOCKET             socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddress    = {0};


    CHECK_INVARIANT(
        socketDescriptor != INVALID_SOCKET,
        "failed to create socket"
    );


    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port   = htons(serverPort);


    if (inet_pton(AF_INET, serverIpAddress, &serverAddress.sin_addr) <= 0)
    {
        log_perror("open_tcp_conn.inet_pton");
        closesocket(socketDescriptor);
        return JK_ERROR;
    }


    int ret = connect(
        socketDescriptor,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)
    );
    if (ret < 0 && errno != EINPROGRESS)
    {
        log_perror("open_tcp_conn.connect");
        closesocket(socketDescriptor);
        return JK_ERROR;
    }


    return socketDescriptor;
}

void close_tcp_conn(int64_t socketDescriptor)
{
    closesocket(socketDescriptor); // NOLINT
}

ssize_t udp_recv(
    udp_socket_t* p_socketStruct,
    uint8_t*      p_recvBuffer,
    size_t        bufferLength,
    address_t*    clientAddress
)
{
    logger_t* logger = current_logger;

    SOCKET                  socketDescriptor  = p_socketStruct->fd;
    struct sockaddr_storage recvAddress       = {0};
    socklen_t               recvAddressLength = sizeof(recvAddress);


    CHECK_INVARIANT(p_socketStruct != NULL, "sock is null");

    CHECK_INVARIANT(p_recvBuffer != NULL, "buf is null");

    CHECK_INVARIANT(bufferLength != 0, "count is 0");

    CHECK_INVARIANT(clientAddress != NULL, "address is null");


    ssize_t bytesReceived = recvfrom(
        socketDescriptor,
        p_recvBuffer,
        bufferLength,
        0,
        (struct sockaddr*)&recvAddress,
        &recvAddressLength
    );
    if ((bytesReceived == SOCKET_ERROR)
        && (WSAGetLastError() == WSAEWOULDBLOCK))
    {
        return JK_WOULD_BLOCK;
    }
    else if (bytesReceived == SOCKET_ERROR)
    {
        log_perror("udp_recv.recvfrom");
        return JK_ERROR;
    }


    memset(clientAddress, 0, sizeof(*clientAddress));


    if (recvAddress.ss_family == AF_INET)
    {
        struct sockaddr_in* p_recvAddress = (struct sockaddr_in*)&recvAddress;


        clientAddress->af = AF_INET;

        clientAddress->src_port = ntohs(p_recvAddress->sin_port);

        clientAddress->src.src_v4 = p_recvAddress->sin_addr;
    }
    else if (recvAddress.ss_family == AF_INET6)
    {
        struct sockaddr_in6* p_recvAddress = (struct sockaddr_in6*)&recvAddress;


        clientAddress->af = AF_INET6;

        clientAddress->src_port = ntohs(p_recvAddress->sin6_port);

        clientAddress->src.src_v6 = p_recvAddress->sin6_addr;
    }
    else
    {
        PANIC("Unsupported address family");
        return JK_ERROR;
    }


    return bytesReceived;
}
