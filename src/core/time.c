#include "time.h"

#include "core/errors.h"
#include "logger/logger.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

jk_timer_heap_t* jk_th_create(size_t capacity) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(capacity > 0, "Bad capacity");

    jk_timer_heap_t *th = calloc(1, sizeof(jk_timer_heap_t));
    if (th == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t");
        return NULL;
    }
    
    jk_timer_t** data = (jk_timer_t**)calloc(capacity, sizeof(*th->data));
    if (data == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t->data");
        free(th);
        return NULL;
    }

    th->data = data;
    th->capacity = capacity;
    th->size = 0;

    return th;
}

void jk_th_destroy(jk_timer_heap_t *th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");
    CHECK_INVARIANT(th->data != NULL, "th is NULL");

    free((void*)th->data);
    free(th);
}

int jk_th_add(jk_timer_heap_t* th, jk_timer_t* timer) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");
    CHECK_INVARIANT(timer != NULL, "timer is NULL");

    if (th->size == th->capacity) {
        return JK_OUT_OF_BUFFER;
    }

    jk_timer_t** data = th->data;
    data[th->size] = timer;

    size_t idx = th->size;
    size_t parent_idx = (idx - 1) / 2;

    for (;;) {
        if (idx == 0) {
            break;
        }

        if (data[parent_idx]->expiry <= data[idx]->expiry) {
            break;
        }

        jk_timer_t* tmp = data[parent_idx];
        data[parent_idx] = data[idx];
        data[idx] = tmp;

        idx = parent_idx;
        parent_idx = (idx - 1) / 2;
    }

    th->size += 1;

    return JK_OK;
}

jk_timer_t* jk_th_peek(jk_timer_heap_t* th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");

    if (th->size == 0) {
        return NULL;
    }

    return th->data[0];
}

jk_timer_t* jk_th_pop(jk_timer_heap_t* th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");

    if (th->size == 0) {
        return NULL;
    }

    jk_timer_t** data = th->data;

    jk_timer_t* result = *data;
    *data = data[th->size - 1];
    data[th->size - 1] = NULL;
    th->size -= 1;

    size_t idx = 0;
    size_t lchild_idx = 2 * idx + 1;
    size_t rchild_idx = lchild_idx + 1;

    for (;;) {
        if (lchild_idx >= th->size) {
            break;
        }

        size_t smallest_child_idx = lchild_idx;
        if (rchild_idx < th->size && 
            data[rchild_idx]->expiry <= data[lchild_idx]->expiry) {
            smallest_child_idx = rchild_idx;
        }

        if (data[idx]->expiry < data[smallest_child_idx]->expiry) {
            break;
        }

        jk_timer_t* tmp = data[smallest_child_idx];
        data[smallest_child_idx] = data[idx];
        data[idx] = tmp;

        idx = smallest_child_idx;
        lchild_idx = 2 * idx + 1;
        rchild_idx = lchild_idx + 1;
    }

    return result;
}
