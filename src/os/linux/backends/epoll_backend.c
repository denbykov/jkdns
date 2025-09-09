#include <core/decl.h>
#include <core/event.h>
#include <core/ev_backend.h>
#include <core/listener.h>
#include <core/connection.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/epoll.h>

#define EPOLL_MAX_EVENTS 512

static struct epoll_event* event_list;
static int epoll_fd = -1;

static int64_t epoll_init();
static int64_t epoll_shutdown();
static int64_t epoll_add_event(event_t* ev);
static int64_t epoll_del_event(event_t* ev);
static int64_t epoll_enable_event(event_t* ev);
static int64_t epoll_disable_event(event_t* ev);
static int64_t epoll_process_events();

ev_backend_t epoll_backend = {
    .name = "epoll",
    .init = epoll_init,
    .shutdown = epoll_shutdown,
    .add_event = epoll_add_event,
    .del_event = epoll_del_event,
    .enable_event = epoll_enable_event,
    .disable_event = epoll_disable_event,
    .process_events = epoll_process_events
};

static int64_t epoll_init() {
    event_list = calloc(EPOLL_MAX_EVENTS, sizeof(struct epoll_event));
    if (event_list == NULL) {
        perror("epoll_init.allocate_event_list");
        return -1;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_init.epoll_create1");
        return -1;
    }

    return 0;
}

static int64_t epoll_shutdown() {
    if (event_list != NULL) {
        free(event_list);
    }

    if (epoll_fd != -1) {
        close(epoll_fd);
    }

    return 0;
}

static int64_t epoll_add_event(event_t* ev) {
    int64_t fd = 0;
    struct epoll_event event;
        
    if (ev->enabled) {
        fprintf(stderr, "epoll_add_event: event is already enabled\n");
        exit(1);
    }

    if (ev->write) {
        event.events = EPOLLOUT | EPOLLET;
    } else {
        event.events = EPOLLIN | EPOLLET;
    }

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "epoll_add_event: event owner is NULL\n");
        exit(1);
    }

    switch (ev->owner.tag) {
        case EV_OWNER_LISTENER:
            fd = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            fd = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            fprintf(stderr, "epoll_add_event: unknown event owner\n");
            exit(1);
    }

    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) { //NOLINT
        perror("epoll_add_event.epoll_ctl");
        return -1;
    }

    ev->enabled = true;

    return 0;
}

static int64_t epoll_del_event(event_t* ev) {
    int64_t fd = 0;

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "epoll_del_event: event owner is NULL\n");
        exit(1);
    }

    switch (ev->owner.tag) {
        case EV_OWNER_LISTENER:
            fd = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            fd = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            fprintf(stderr, "epoll_del_event: unknown event owner\n");
            exit(1);
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) { //NOLINT
        perror("epoll_del_event.epoll_ctl");
        return -1;
    }

    ev->enabled = false;

    return 0;
}

static int64_t epoll_enable_event(event_t* ev) {
    int64_t fd = 0;
    struct epoll_event event;

    if (ev->enabled) {
        fprintf(stderr, "epoll_enable_event: event is already enabled\n");
        exit(1);
    }

    if (ev->write) {
        event.events = EPOLLOUT | EPOLLET;
    } else {
        event.events = EPOLLIN | EPOLLET;
    }

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "epoll_enable_event: event owner is NULL\n");
        exit(1);
    }

    switch (ev->owner.tag) {
        case EV_OWNER_LISTENER:
            fd = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            fd = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            fprintf(stderr, "epoll_enable_event: unknown event owner\n");
            exit(1);
    }

    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) { //NOLINT
        perror("epoll_enable_event.epoll_ctl");
        return -1;
    }

    ev->enabled = true;

    return 0;
}

static int64_t epoll_disable_event(event_t* ev) {
    int64_t fd = 0;
    struct epoll_event event;

    if (!ev->enabled) {
        fprintf(stderr, "epoll_disable_event: event is already disabled\n");
        exit(1);
    }

    event.events = 0;

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "epoll_disable_event: event owner is NULL\n");
        exit(1);
    }

    switch (ev->owner.tag) {
        case EV_OWNER_LISTENER:
            fd = ((listener_t*)ev->owner.ptr)->fd;
            break;
        case EV_OWNER_CONNECTION:
            fd = ((connection_t*)ev->owner.ptr)->fd;
            break;
        default:
            fprintf(stderr, "epoll_disable_event: unknown event owner\n");
            exit(1);
    }

    event.data.ptr = ev;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) { //NOLINT
        perror("epoll_disable_event.epoll_ctl");
        return -1;
    }

    ev->enabled = false;

    return 0;
}

static int64_t epoll_process_events() {
    // ToDo: add timeout
    int nfds = epoll_wait(epoll_fd, event_list, EPOLL_MAX_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll_process_events.epoll_wait");
        return -1;
    }

    for (int n = 0; n < nfds; ++n) {
        if (event_list[n].events & (EPOLLERR | EPOLLHUP)) {
            // ToDo: handle errors
        }

        if (event_list[n].data.ptr == NULL) {
            fprintf(stderr, "epoll_process_events: event is NULL\n");
            exit(1);
        }

        event_t* ev = (event_t*)event_list[n].data.ptr;

        if (ev->handler == NULL) {
            fprintf(stderr, "epoll_process_events: event handler is NULL\n");
            exit(1);
        }

        ev->handler(ev);
    }

    return 0;
}
