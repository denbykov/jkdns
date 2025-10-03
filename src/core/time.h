#pragma once

#include "decl.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

typedef void (*timer_handler)(void* data);

int64_t jk_now();

typedef struct {
    int64_t expiry;
    timer_handler handler;
    void *data;
    bool enabled;
} jk_timer_t;

typedef struct {
    jk_timer_t **data;
    size_t size;
    size_t capacity;
} jk_timer_heap_t;

jk_timer_heap_t* jk_th_create(size_t capacity);
void jk_th_destroy(jk_timer_heap_t *th);
int jk_th_add(jk_timer_heap_t* th, jk_timer_t* timer);
jk_timer_t* jk_th_peek(jk_timer_heap_t* th);
jk_timer_t* jk_th_pop(jk_timer_heap_t* th);
