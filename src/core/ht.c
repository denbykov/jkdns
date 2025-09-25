#include "ht.h"
#include "core/errors.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "logger/logger.h"
#include <unistd.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static int connection_ht_resize(
    connection_ht_t* ht,
    size_t capacity);

static int connection_ht_insert_impl(
    connection_ht_t* ht,
    connection_key_t* key,
    connection_t* data);

static inline bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static size_t connection_hash(const connection_key_t *key) {
    size_t hash = FNV_OFFSET_BASIS;

    // hash address family
    hash ^= key->af;
    hash *= FNV_PRIME;

    // hash port (little-endian safe, works byte by byte)
    uint16_t port = key->src_port;
    const uint8_t *pport = (const uint8_t *)&port;
    for (size_t i = 0; i < sizeof(port); i++) {
        hash ^= pport[i];
        hash *= FNV_PRIME;
    }

    if (key->af == 4) {
        const uint8_t *p = (const uint8_t *)&key->src.src_v4;
        for (size_t i = 0; i < sizeof(struct in_addr); i++) {
            hash ^= p[i];
            hash *= FNV_PRIME;
        }
    } else {
        const uint8_t *p = (const uint8_t *)&key->src.src_v6;
        for (size_t i = 0; i < sizeof(struct in6_addr); i++) {
            hash ^= p[i];
            hash *= FNV_PRIME;
        }
    }
    
    return hash;
}

static int connection_equal(const connection_key_t *a, const connection_key_t *b) {
    if (a->af != b->af) return 0;
    if (a->src_port != b->src_port) return 0;
    if (a->af == 4) {
        return memcmp(&a->src.src_v4, &b->src.src_v4, sizeof(struct in_addr)) == 0;
    } else {
        return memcmp(&a->src.src_v6, &b->src.src_v6, sizeof(struct in6_addr)) == 0;
    }
}

connection_ht_t* connection_ht_create(size_t capacity) {
    logger_t* logger = current_logger;
    CHECK_INVARIANT(is_power_of_two(capacity), "capacity is not power of two");

    connection_ht_t* ht = calloc(1, sizeof(connection_ht_t));
    if (ht == NULL) {
        return NULL;
    }
    
    connection_ht_slot_t* slots = calloc(capacity, sizeof(connection_ht_slot_t));
    if (slots == NULL) {
        free(ht);
        return NULL;
    }

    ht->slots = slots;
    ht->capacity = capacity;
    ht->size = 0;
    ht->tombstones = 0;

    return ht;
}

void connection_ht_destroy(connection_ht_t* ht) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ht != NULL, "ht is null");
    CHECK_INVARIANT(ht->slots != NULL, "ht slots is null");

    free(ht->slots);
    free(ht);
}

int connection_ht_resize(connection_ht_t* ht, size_t capacity) { // NOLINT
    logger_t* logger = current_logger;

    CHECK_INVARIANT(is_power_of_two(capacity), "capacity is not power of two");

    connection_ht_slot_t* slots = calloc(capacity, sizeof(connection_ht_slot_t));
    if (slots == NULL) {
        return -1;
    }

    connection_ht_t new_ht;
    new_ht.slots = slots;
    new_ht.capacity = capacity;
    new_ht.size = 0;
    new_ht.tombstones = 0;

    connection_ht_slot_t* old_slots = ht->slots;

    for (size_t i = 0; i < ht->capacity; i++) {
        if (old_slots[i].state == HTS_OCCUPIED) {
            int res = 
                connection_ht_insert_impl(
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

int connection_ht_insert(connection_ht_t* ht, connection_key_t* key, connection_t* data) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ht != NULL, "ht is null");
    CHECK_INVARIANT(key != NULL, "key is null");

    if ((double)(ht->size + 1) >= (double)ht->capacity * 0.7) {
        int res = connection_ht_resize(ht, ht->capacity * 2);
        if (res != JK_OK) {
            return res;
        }
    } else if ((double)(ht->tombstones) >= (double)ht->capacity * 0.2) {
        int res = connection_ht_resize(ht, ht->capacity * 2);
        if (res != JK_OK) {
            return res;
        }
    }

    return connection_ht_insert_impl(ht, key, data);
}

int connection_ht_insert_impl(
    connection_ht_t* ht,
    connection_key_t* key,
    connection_t* data) {
    
    size_t idx = connection_hash(key) & (ht->capacity - 1);
    connection_ht_slot_t* slots = ht->slots;
    connection_ht_slot_t* insertion_pos = NULL;
    
    for (;;) {
        connection_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY && insertion_pos == NULL) {
            insertion_pos = slot;
            break;
        }

        if (slot->state == HTS_EMPTY && insertion_pos != NULL) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && connection_equal(&slot->key, key)) {
            return JK_OCCUPIED;
        }

        if (slot->state == HTS_TOMBSTONE && insertion_pos == NULL) {
            insertion_pos = slot;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    if (insertion_pos->state == HTS_TOMBSTONE) {
        ht->tombstones -= 1;
    }
    
    insertion_pos->state = HTS_OCCUPIED;
    insertion_pos->key = *key;
    insertion_pos->value = data;

    ht->size += 1;

    return JK_OK;
}

connection_t* connection_ht_lookup(
    connection_ht_t* ht,
    connection_key_t* key) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ht != NULL, "ht is null");
    CHECK_INVARIANT(key != NULL, "key is null");

    connection_t *result = NULL;
    
    size_t idx = connection_hash(key) & (ht->capacity - 1);
    connection_ht_slot_t* slots = ht->slots;
    
    for (;;) {
        connection_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && connection_equal(&slot->key, key)) {
            result = slot->value;
            break;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    return result;
}

int connection_ht_delete(
    connection_ht_t* ht,
    connection_key_t* key) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(ht != NULL, "ht is null");
    CHECK_INVARIANT(key != NULL, "key is null");
    
    size_t idx = connection_hash(key) & (ht->capacity - 1);
    connection_ht_slot_t* slots = ht->slots;
    
    for (;;) {
        connection_ht_slot_t* slot = slots + idx;

        if (slot->state == HTS_EMPTY) {
            return JK_NOT_FOUND;
        }

        if (slot->state == HTS_OCCUPIED && connection_equal(&slot->key, key)) {
            slot->state = HTS_TOMBSTONE;
            slot->value = NULL;

            ht->tombstones += 1;
            ht->size -= 1;

            return JK_OK;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    return JK_OK;
}
