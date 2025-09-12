#include "core/decl.h"
#include <core/ev_backend.h>
#include <core/event.h>
#include <core/listener.h>
#include <logger/logger.h>

#include <settings/settings.h>

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

    for (;;) {
        ev_backend->process_events();
    }

    free(settings);
    release_listener(l);
    ev_backend->shutdown();

    return 0;
}
