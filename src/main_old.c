#include <asm-generic/errno-base.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <uthash.h>
#include <stdbool.h>

#include <fcntl.h>

#include <sys/epoll.h>

#define PORT "9034"
#define MAX_EVENTS 10

const char *inet_ntop2(void *addr, char *buf, size_t size)
{
    struct sockaddr_storage *sas = addr;
    struct sockaddr_in *sa4;
    struct sockaddr_in6 *sa6;
    void *src;

    switch (sas->ss_family) {
        case AF_INET:
            sa4 = addr;
            src = &(sa4->sin_addr);
            break;
        case AF_INET6:
            sa6 = addr;
            src = &(sa6->sin6_addr);
            break;
        default:
            return NULL;
    }

    return inet_ntop(sas->ss_family, src, buf, size);
}

int start_listening() {
    struct addrinfo hints, *ai, *p;
    int yes = 1;
    int rv = 0;
    int listener = 0;

    memset(&hints, 0, sizeof(hints)); // NOLINT
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv)); // NOLINT
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n"); //NOLINT
        exit(2);
    }

    freeaddrinfo(ai);

    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    return listener;
}

void setnonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return; // handle error

    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("setnonblocking");
        exit(1);
    }
}

typedef struct {
    int id;
    uint8_t* buffer;
    uint64_t size;
    UT_hash_handle hh;
} session_t;

int main() {
    int listener = start_listening();

    int conn_sock, nfds, epollfd;
    struct epoll_event ev, events[MAX_EVENTS];

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listener;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listener, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(1);
    }

    session_t* sessions = NULL;

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(1);
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listener) {
                socklen_t addrlen;
                struct sockaddr_storage addr;

                addrlen = sizeof(addr);

                conn_sock = accept(listener, (struct sockaddr*)&addr, &addrlen);

                if (conn_sock == -1) {
                    perror("accept");
                    exit(1);
                } 

                setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = conn_sock;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(1);
                }

                session_t* s = malloc(sizeof(session_t));
                s->id = conn_sock;
                s->buffer = NULL;
                s->size = 0;
                HASH_ADD_INT(sessions, id, s);
            } else {
                session_t* found;
                HASH_FIND_INT(sessions, &events[n].data.fd, found);
                if (!found) {
                    perror("session is not found");
                    exit(1);
                }

                uint8_t buffer[4];
                uint64_t taken = 0;

                if (events[n].events & EPOLLIN) {
                    printf("EPOLLIN\n");
                    
                    while (true) {
                        int nr = read(events[n].data.fd, buffer + taken, sizeof(buffer) - taken); //NOLINT

                        taken += nr;
                        
                        if (nr == -1 && errno == EAGAIN) {
                            printf("block\n");
                            break;
                        }
                        
                        if (nr == -1) {
                            perror("read failed");
                            exit(1);
                        }
                        
                        if (nr == 0) {
                            printf("connection closed\n");
                            exit(1);
                        }
                        
                        if (sizeof(buffer) - taken <= 0) {
                            break;
                        }
                    }

                    printf("EPOLLIN end\n");
                }

                if (events[n].events & EPOLLOUT) {
                    printf("EPOLLOUT\n");

                    uint64_t offset = 0;
                    while (true) {
                        int nw = write(events[n].data.fd, buffer + offset, taken - offset);

                        if (nw == -1 && errno == EAGAIN) {
                            printf("block\n");
                            break;
                        }

                        if (nw == -1) {
                            perror("write failed");
                            exit(1);
                        }

                        offset += nw;

                        if (taken == offset) {
                            break;
                        }
                    }

                    printf("EPOLLOUT end\n");
                }
            }
        }
    }


    return 0;
}