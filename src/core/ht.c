#include "ht.h"
#include "core/errors.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <logger/logger.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static int session_ht_resize(
    session_ht_t* ht,
    size_t capacity);

static int session_ht_insert_impl(
    session_ht_t* ht,
    session_key_t* key,
    void* data);

static inline bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static size_t session_hash(const session_key_t *key) {
    const uint8_t *p = (const uint8_t*)key;
    size_t len = (key->af == 4) ? 1+2+4 : 1+2+16;
    size_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static int session_equal(const session_key_t *a, const session_key_t *b) {
    if (a->af != b->af) return 0;
    if (a->src_port != b->src_port) return 0;
    if (a->af == 4) {
        return memcmp(&a->src.src_v4, &b->src.src_v4, sizeof(struct in_addr)) == 0;
    } else {
        return memcmp(&a->src.src_v6, &b->src.src_v6, sizeof(struct in6_addr)) == 0;
    }
}

session_ht_t* session_ht_create(size_t capacity) {
    logger_t* logger = current_logger;
    CHECK_INVARIANT(is_power_of_two(capacity), "capacity is not power of two");

    session_ht_t* ht = calloc(1, sizeof(session_ht_t));
    if (ht == NULL) {
        return NULL;
    }
    
    session_ht_slot_t* slots = calloc(capacity, sizeof(session_ht_slot_t));
    if (slots == NULL) {
        free(ht);
        return NULL;
    }

    ht->slots = slots;
    ht->capacity = capacity;

    return ht;
}

void session_ht_destroy(session_ht_t* ht) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ht->slots != NULL, "ht slots is null");
    CHECK_INVARIANT(ht != NULL, "ht is null");

    free(ht->slots);
    free(ht);
}

int session_ht_resize(session_ht_t* ht, size_t capacity) { // NOLINT
    logger_t* logger = current_logger;

    CHECK_INVARIANT(is_power_of_two(capacity), "capacity is not power of two");

    session_ht_slot_t* slots = calloc(capacity, sizeof(session_ht_slot_t));
    if (slots == NULL) {
        return -1;
    }

    session_ht_t new_ht;
    new_ht.slots = slots;
    new_ht.capacity = capacity;
    new_ht.size = 0;

    session_ht_slot_t* old_slots = ht->slots;

    for (size_t i = 0; i < ht->capacity; i++) {
        if (old_slots[i].state == HTS_OCCUPIED) {
            int res = 
                session_ht_insert_impl(
                    &new_ht,
                    &old_slots[i].key,
                    old_slots[i].value);
            
            if (res != 0) {
                free(slots);
                return res;
            }
        }
    }

    *ht = new_ht;
    free(old_slots);

    return JK_OK;
}

int session_ht_insert(session_ht_t* ht, session_key_t* key, void* data) {
    if ((double)(ht->size + 1) >= (double)ht->capacity * 0.7) {
        int res = session_ht_resize(ht, ht->capacity * 2);
        if (res != JK_OK) {
            return res;
        }
    }

    return session_ht_insert_impl(ht, key, data);
}

int session_ht_insert_impl(
    session_ht_t* ht,
    session_key_t* key,
    void* data) {
    
    size_t idx = session_hash(key) & (ht->capacity - 1);
    session_ht_slot_t* slots = ht->slots;
    session_ht_slot_t* insertion_pos = NULL;
    
    for (;;) {
        session_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY && insertion_pos == NULL) {
            insertion_pos = slot;
            break;
        }

        if (slot->state == HTS_EMPTY && insertion_pos != NULL) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && session_equal(&slot->key, key)) {
            insertion_pos = slot;
            break;
        }

        if (slot->state == HTS_OCCUPIED && 
            !session_equal(&slot->key, key) &&
            insertion_pos != NULL) {
            break;
        }

        if (slot->state == HTS_TOMBSTONE && insertion_pos == NULL) {
            insertion_pos = slot;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    insertion_pos->state = HTS_OCCUPIED;
    insertion_pos->key = *key;
    insertion_pos->value = data;

    return JK_OK;
}

void* session_ht_lookup(
    session_ht_t* ht,
    session_key_t* key) {

    void *result = NULL;
    
    size_t idx = session_hash(key) & (ht->capacity - 1);
    session_ht_slot_t* slots = ht->slots;
    
    for (;;) {
        session_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && session_equal(&slot->key, key)) {
            result = slot->value;
            break;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    return result;
}

int session_ht_delete(
    session_ht_t* ht,
    session_key_t* key) {
    
    size_t idx = session_hash(key) & (ht->capacity - 1);
    session_ht_slot_t* slots = ht->slots;
    
    for (;;) {
        session_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY) {
            return JK_NOT_FOUND;
        }

        if (slot->state == HTS_OCCUPIED && session_equal(&slot->key, key)) {
            memset(slot, 0, sizeof(session_ht_slot_t));
            slot->state = HTS_TOMBSTONE;
            return JK_OK;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    return JK_OK;
}
