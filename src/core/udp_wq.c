#include "udp_wq.h"
#include "core/decl.h"
#include "core/errors.h"
#include "core/ht.h"
#include "core/htt.h"
#include "logger/logger.h"
#include "event.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define CHECK_EV(ev) \
    CHECK_INVARIANT((ev) != NULL, "ev is NULL"); \
    CHECK_INVARIANT( \
        (ev)->owner.tag == EV_OWNER_CONNECTION, \
        "event owner is not a connection"); \
    CHECK_INVARIANT( \
        (ev)->owner.ptr != NULL, \
        "event owner is NULL");

#define CHECK_CONN(conn) \
    CHECK_INVARIANT( \
        (conn)->handle.type == CONN_TYPE_UDP, \
        "conn handle type is not UDP"); \
    CHECK_INVARIANT( \
        (conn)->handle.data.sock != NULL, \
        "conn handle sock is not NULL");

static inline bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

udp_wq_t* udp_wq_create(size_t capacity) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(is_power_of_two(capacity), "capacity is not power of two");

    udp_wq_t *wq = calloc(1, sizeof(udp_wq_t));
    if (wq == NULL) {
        return NULL;
    }

    event_t** data = (event_t**)calloc(capacity, sizeof(event_t*));
    if (data == NULL) {
        free(wq);
        return NULL;
    }

    wq->data = data;
    wq->head = data;
    wq->capacity = capacity;
    wq->size = 0;

    udp_wq_ht_t *ht = udp_wq_ht_create(capacity * 2);
    if (ht == NULL) {
        free((void*)data);
        free(wq);
        return NULL;
    }

    wq->ht = ht;

    return wq;
}

void udp_wq_destroy(udp_wq_t* wq) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(wq != NULL, "wq is NULL");
    CHECK_INVARIANT(wq->data != NULL, "wq->data is NULL");
    CHECK_INVARIANT(wq->ht != NULL, "wq->ht is NULL");

    udp_wq_ht_destroy(wq->ht);
    free((void*)wq->data);
    free(wq);
}

int udp_wq_add(udp_wq_t* wq, event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(wq != NULL, "wq is NULL");
    CHECK_EV(ev);

    connection_t *conn = (connection_t*)ev->owner.ptr;
    CHECK_CONN(conn);

    if (wq->size == wq->capacity) {
        return JK_OUT_OF_BUFFER;
    }

    connection_key_t *key = &(conn->address);
    udp_wq_ht_t* ht = wq->ht;

    event_t **existing_ev = udp_wq_ht_lookup(ht, key);
    if (existing_ev != NULL) {
        return JK_OCCUPIED;
    }

    event_t** insertion_pos = wq->data + wq->size;
    
    int res = udp_wq_ht_insert(ht, key, insertion_pos);
    if (res != JK_OK) {
        return res;
    }

    *insertion_pos = ev;
    wq->size += 1;
    
    return JK_OK;
}

event_t* udp_wq_pop_front(udp_wq_t* wq) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(wq != NULL, "wq is NULL");

    if (wq->size == 0) {
        return NULL;
    }

    event_t* ev = *wq->head;

    size_t next_head_idx = (wq->head - wq->data + 1);
    if (next_head_idx == wq->size) {
        next_head_idx = 0;
    }
    wq->head = wq->data + next_head_idx;
    
    return ev;
}

int udp_wq_remove(udp_wq_t* wq, event_t* ev) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(wq != NULL, "wq is NULL");
    udp_wq_ht_t* ht = wq->ht;

    CHECK_EV(ev);
    connection_t *conn = (connection_t*)ev->owner.ptr;
    CHECK_CONN(conn);

    connection_key_t *key = &(conn->address);

    event_t** pos = udp_wq_ht_lookup(ht, key);
    if (pos == NULL) {
        return JK_NOT_FOUND;
    }

    int res = udp_wq_ht_delete(ht, key);
    CHECK_INVARIANT(res == JK_OK, "ht delete failed!");

    if (wq->size == 1) {
        *pos = NULL;
        wq->size = 0;
        wq->head = wq->data;
        return JK_OK;
    }
    
    // replace the removed element with the last valid one to keep [0..size-1] packed
    event_t** last_element = wq->data + wq->size - 1;
    CHECK_INVARIANT(*last_element != NULL, "last element is NULL");

    event_t* last_ev = *last_element;

    connection_t *le_conn = (connection_t*)last_ev->owner.ptr;
    connection_key_t *le_key = &(le_conn->address);
    
    res = udp_wq_ht_insert(ht, le_key, pos);
    CHECK_INVARIANT(res == JK_OK, "ht insert failed!");

    if (wq->head == last_element) {
        wq->head = wq->data;
    }

    *pos = last_ev;
    *last_element = NULL;
    wq->size -= 1;

    return JK_OK;
}
