#pragma once

#include "decl.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*timer_handler)(void* data);

int64_t jk_now();

typedef struct {
    // time in ms
    int64_t expiry;
    timer_handler handler;
    void *data;
    bool enabled;
} jk_timer_t;

void jk_timer_start(jk_timer_t* timer, int64_t delay_ms);

typedef struct {
    jk_timer_t *data;
    size_t size;
    size_t capacity;
} jk_timer_heap_t;

jk_timer_heap_t* jk_th_create(size_t capacity);
void jk_th_destroy(jk_timer_heap_t* th);
jk_timer_t* jk_th_add(jk_timer_heap_t* th, jk_timer_t timer);
jk_timer_t* jk_th_peek(jk_timer_heap_t* th);
void jk_th_pop(jk_timer_heap_t* th);
// debug function that prints all timers
void jk_th_dump(jk_timer_heap_t* th);
