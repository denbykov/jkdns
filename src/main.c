#include "core/decl.h"
#include "core/ev_backend.h"
#include "core/event.h"
#include "core/listener.h"
#include "core/udp_socket.h"
#include "settings/settings.h"
#include "logger/logger.h"
#include "udp_socket/udp_socket.h"

#include <stdlib.h>
#include <stdbool.h>

extern ev_backend_t epoll_backend;

int main(int argc, char *argv[]) {
    settings_t* settings = malloc(sizeof(settings_t));
    init_settings(settings);

    if (parse_args(argc, argv, settings) == -1) {
        return -1;
    }

    if (validate_settings(settings) == -1) {
        return -1;
    }

    dump_settings(stdout, settings);
    current_settings = settings;

    current_logger = init_logger(settings);

    ev_backend = &epoll_backend;

    if (ev_backend->init() == -1) {
        return -1;
    }

    // Register listener
    listener_t* l = make_listener();
    if (l == NULL || l->error == true) {
        release_listener(l);
        return -1;
    }
    
    event_t ev;
    init_event(&ev);
    ev.owner.ptr = l;
    ev.owner.tag = EV_OWNER_LISTENER;
    ev.write = false;
    ev.handler = accept_handler;
    
    l->accept = &ev;
    
    ev_backend->add_event(&ev);

    // Register udp socket
    udp_socket_t* usock = make_udp_socket();
    if (usock == NULL || usock->error == true) {
        release_udp_socket(usock);
        return -1;
    }

    event_t uev;
    init_event(&uev);
    uev.owner.ptr = usock;
    uev.owner.tag = EV_OWNER_USOCK;
    uev.write = false;
    uev.handler = udp_ev_handler;
    usock->ev = &uev;
    
    ev_backend->add_udp_sock(usock);
    
    // Mainloop
    for (;;) {
        ev_backend->process_events();
    }
    
    ev_backend->del_udp_sock(usock);

    free(settings);
    release_listener(l);
    ev_backend->shutdown();

    return 0;
}
