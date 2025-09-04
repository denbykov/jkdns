#include "core/decl.h"
#include <core/ev_backend.h>
#include <core/event.h>
#include <core/listener.h>

#include <stdio.h>
#include <stdbool.h>

extern ev_backend_t epoll_backend;

static void accept_handler(event_t* ev) {
    (void)ev;
    printf("Handling new connection attempt\n");
}

int main() {
    ev_backend_t* ev_backend = &epoll_backend;

    if(ev_backend->init() == -1) {
        return -1;
    }

    listener_t* l = make_listener();
    if (l == NULL || l->error == true) {
        release_listener(l);
        return -1;
    }

    event_t ev;
    ev.owner.ptr = l;
    ev.owner.tag = EV_OWNER_LISTENER;
    ev.write = false;
    ev.handler = accept_handler;

    l->accept = &ev;

    ev_backend->add_event(&ev);

    for (;;) {
        ev_backend->process_events();
    }

    release_listener(l);
    ev_backend->shutdown();

    return 0;
}