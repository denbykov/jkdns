#include "udp_socket.h"
#include "core/decl.h"
#include "logger/logger.h"
#include "core/event.h"

void udp_ev_handler(event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ev->owner.tag == EV_OWNER_USOCK, "event owner is not a udp socket");
    udp_socket_t* sock = ev->owner.ptr;

    CHECK_INVARIANT(sock->readable || sock->writable, "udp socket is neither writable or readable");
    
    if (sock->readable && sock->writable) {
        log_info("udp ev of the soul and heart");
    } else if (sock->readable) {
        log_info("udp ev of the soul");
    } else {
        log_info("udp ev of the heart");
    }
}
