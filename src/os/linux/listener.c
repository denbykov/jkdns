#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <core/decl.h>
#include <core/listener.h>
#include <core/event.h>
#include <session/tcp.h>
#include <settings/settings.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>

#define LISTEN_QUEUE 10

listener_t* make_listener(settings_t *s) {
    listener_t* l = calloc(1, sizeof(listener_t));
    if (l == NULL) {
        perror("make_listener.allocate_event_list");
        return NULL;
    }

    l->accept = NULL;
    l->fd = -1;
    
    struct sockaddr_in server_sockaddr;
    int yes = 1;
    int fd = 0;

    server_sockaddr.sin_family=AF_INET;
	server_sockaddr.sin_port = htons(s->port);
	server_sockaddr.sin_addr.s_addr=INADDR_ANY;
	memset(&(server_sockaddr.sin_zero),0,8);

    fd = socket(AF_INET, SOCK_STREAM,0);
    if (fd < 0) {
        perror("make_listener.socket");
        l->error = true;
        return l;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("make_listener.setsockopt");
        close(fd);
        l->error = true;
        return l;
    }
    
    if (bind(fd, (struct sockaddr *)&server_sockaddr,sizeof(struct sockaddr)) < 0) {
        perror("make_listener.bind");
        close(fd);
        l->error = true;
        return l;
    }

    l->fd = fd;
    l->bound = true;

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
            close(l->fd); //NOLINT
        }

        free(l);
    }
}

void accept_handler(event_t *ev) {
    int fd = -1;

    if (ev->owner.ptr == NULL) {
        fprintf(stderr, "accept_handler: event owner is NULL\n");
        exit(1);
    }

    switch (ev->owner.tag) {
        case EV_OWNER_LISTENER:
            fd = ((listener_t*)ev->owner.ptr)->fd; //NOLINT
            break;
        default:
            fprintf(stderr, "accept_handler: unexpected event owner\n");
            exit(1);
    }
    
    for(;;) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(struct sockaddr_storage);

        int conn_fd = accept(fd, (struct sockaddr*)&addr, &addrlen);

        if (conn_fd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (conn_fd == -1) {
            perror("accept_handler.accept");
            exit(1);
        }

        int flags = fcntl(conn_fd, F_GETFL, 0);
        if (flags == -1) {
            perror("accept_handler.fcntl_get_flags");
            exit(1);
        };
        
        flags |= O_NONBLOCK;
        if (fcntl(conn_fd, F_SETFL, flags) == -1) {
            perror("accept_handler.fcntl_set_non_blocking");
            exit(1);
        }

        handle_new_connection(conn_fd);
    }
}
