#include "connection/connection.h"
#include "core/errors.h"
#include "logger/logger.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int64_t fill_address(struct address_s *addr, const char *ip, uint16_t port) {
    logger_t *logger = current_logger;

    CHECK_INVARIANT(addr != NULL, "addr is NULL!");
    CHECK_INVARIANT(ip != NULL, "ip is NULL!");

    memset(addr, 0, sizeof(*addr));

    if (inet_pton(AF_INET, ip, &addr->src.src_v4) == 1) {
        addr->af = AF_INET;
        addr->src_port = port;
        return JK_OK;
    }

    if (inet_pton(AF_INET6, ip, &addr->src.src_v6) == 1) {
        addr->af = AF_INET6;
        addr->src_port = port;
        return JK_OK;
    }

    return JK_ERROR;
}