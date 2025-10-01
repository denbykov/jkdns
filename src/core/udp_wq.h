#pragma once

#include "core/decl.h"
#include "htt.h"

#include <stddef.h>

typedef struct {
    udp_wq_ht_t *ht;
    event_t **data;
    event_t **head;
    size_t size;
    size_t capacity;
} udp_wq_t;

udp_wq_t* udp_wq_create(size_t capacity);
void udp_wq_destroy(udp_wq_t* wq);
int udp_wq_add(udp_wq_t* wq, event_t* ev);

// returns front element and reschedules it as last element
// please note that order is not stable!
// returns NULL if no elements left in the queue
event_t* udp_wq_pop_front(udp_wq_t* wq);
int udp_wq_remove(udp_wq_t* wq, event_t* ev);
