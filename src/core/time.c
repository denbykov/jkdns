#include "time.h"

#include "core/errors.h"
#include "logger/logger.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static jk_timer_t* get_new_timer(jk_timer_pool_t* pool);
static void release_timer(jk_timer_pool_t* pool, jk_timer_t* timer);

void jk_timer_start(jk_timer_t *timer, int64_t delay_ms) {
    if (!timer) return;
    timer->expiry = jk_now() + delay_ms;
    timer->enabled = true;
}

jk_timer_heap_t* jk_th_create(size_t capacity) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(capacity > 0, "Bad capacity");

    jk_timer_heap_t *th = calloc(1, sizeof(jk_timer_heap_t));
    if (th == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t");
        return NULL;
    }
    
    jk_timer_t** data = (jk_timer_t**)calloc(capacity, sizeof(jk_timer_t*));
    if (data == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t->data");
        free(th);
        return NULL;
    }

    th->data = data;
    th->capacity = capacity;
    th->size = 0;

    jk_timer_t* pool_data = (jk_timer_t*)calloc(capacity, sizeof(jk_timer_t));
    if (pool_data == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t->pool->data");
        free((void*)data);
        free(th);
        return NULL;
    }

    bool* pool_used = (bool*)calloc(capacity, sizeof(bool));
    if (pool_used == NULL) {
        log_perror("jk_th_create.allocate_jk_timer_heap_t->pool->used");
        free(pool_data);
        free((void*)data);
        free(th);
        return NULL;
    }

    th->pool.data = pool_data;
    th->pool.used = pool_used;
    th->pool.capacity = capacity;

    return th;
}

void jk_th_destroy(jk_timer_heap_t *th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");
    CHECK_INVARIANT(th->data != NULL, "th->data is NULL");
    CHECK_INVARIANT(th->pool.data != NULL, "th->pool.data is NULL");
    CHECK_INVARIANT(th->pool.used != NULL, "th->pool.used is NULL");

    free(th->pool.data);
    free(th->pool.used);
    free((void*)th->data);
    free(th);
}

static jk_timer_t* get_new_timer(jk_timer_pool_t* pool) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(pool != NULL, "pool is NULL");

    for (size_t i = 0; i < pool->capacity; i++) {
        if (!pool->used[i]) {
            pool->used[i] = true;
            return pool->data + i;
        }
    }

    return NULL;
}

static void release_timer(jk_timer_pool_t* pool, jk_timer_t* timer) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(pool != NULL, "pool is NULL");
    CHECK_INVARIANT(timer != NULL, "timer is NULL");
    CHECK_INVARIANT(
        timer >= pool->data && timer < pool->data + pool->capacity,
        "timer is out of pool");
    
    size_t idx = timer - pool->data;
    pool->used[idx] = false;
}

jk_timer_t* jk_th_add(jk_timer_heap_t* th, jk_timer_t user_timer) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");
    
    if (th->size == th->capacity) {
        return NULL;
    }
    
    jk_timer_t* timer = get_new_timer(&th->pool);
    CHECK_INVARIANT(timer != NULL, "get_new_timer failed");
    memcpy(timer, &user_timer, sizeof(jk_timer_t));

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

    return timer;
}

jk_timer_t* jk_th_peek(jk_timer_heap_t* th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");

    if (th->size == 0) {
        return NULL;
    }

    return *th->data;
}

void jk_th_pop(jk_timer_heap_t* th) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(th != NULL, "th is NULL");

    if (th->size == 0) {
        return;
    }

    jk_timer_t** data = th->data;

    jk_timer_t* timer = *data;
    release_timer(&th->pool, timer);

    *data = data[th->size - 1];
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

        if (data[idx]->expiry <= data[smallest_child_idx]->expiry) {
            break;
        }

        jk_timer_t* tmp = data[smallest_child_idx];
        data[smallest_child_idx] = data[idx];
        data[idx] = tmp;

        idx = smallest_child_idx;
        lchild_idx = 2 * idx + 1;
        rchild_idx = lchild_idx + 1;
    }

    return;
}

void jk_th_debug_dump(const jk_timer_heap_t* th) {
    logger_t* logger = current_logger;

    if (!th) {
        log_debug("[jk_th_debug_dump] th is NULL");
        return;
    }

    log_debug("=== jk_timer_heap_t DUMP ===");
    log_debug("Heap size: %zu / %zu", th->size, th->capacity);
    log_debug("--- Heap (min-heap order) ---");

    for (size_t i = 0; i < th->size; i++) {
        jk_timer_t* t = th->data[i];
        if (t)
            log_debug("[%2zu] expiry=%" PRId64 " enabled=%d data=%p handler=%p",
                   i, t->expiry, t->enabled, t->data, (void*)t->handler);
        else
            log_debug("[%2zu] <NULL>", i);
    }

    log_debug("--- Pool ---");
    for (size_t i = 0; i < th->pool.capacity; i++) {
        bool used = th->pool.used[i];
        jk_timer_t* t = &th->pool.data[i];
        log_debug("[%2zu] %s", i, used ? "USED " : "FREE ");
        if (used) {
            log_debug(" expiry=%" PRId64 " enabled=%d data=%p handler=%p",
                   t->expiry, t->enabled, t->data, (void*)t->handler);
        }
        log_debug("");
    }

    log_debug("============================");
}
