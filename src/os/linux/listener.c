#include <core/decl.h>
#include <core/listener.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT "9034"
#define LISTEN_QUEUE 10

listener_t* make_listener() {
    listener_t* l = calloc(1, sizeof(listener_t));
    if (l == NULL) {
        perror("make_listener.allocate_event_list");
        return NULL;
    }

    l->accept = NULL;
    l->fd = -1;
    
    struct addrinfo hints, *ai, *p;
    int yes = 1;
    int rv = 0;
    int fd = 0;

    memset(&hints, 0, sizeof(hints)); // NOLINT
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv)); // NOLINT
        l->error = true;
        return l;
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("make_listener.setsockopt");
            l->error = true;
            return l;
        }
        
        if (bind(fd, p->ai_addr, p->ai_addrlen) < 0) {
            close(fd);
            continue;
        }

        l->fd = fd;
        l->bound = true;

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n"); // NOLINT
        l->error = true;
        return l;
    }

    freeaddrinfo(ai);

    if (listen(fd, LISTEN_QUEUE) == -1) {
        perror("listen");
        l->error = true;
        return l;
    }

    l->listening = true;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("make_listener.fcntl_get_flags");
        l->error = true;
        return l;
    };
    
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("make_listener.fcntl_set_non_blocking");
        l->error = true;
        return l;
    }

    l->non_blocking = true;

    return l;
}

void release_listener(listener_t *l) {
    if (l != NULL) {
        if (l->fd != -1) {
            close(l->fd);
        }

        free(l);
    }
}