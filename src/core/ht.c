#include "ht.h"

#include "core/errors.h"
#include "logger/logger.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#define CHECK_INVARIANT_HTE(cond, msg) \
    CHECK_INVARIANT(cond, "%s: %s", hte->ht_name, msg)

static int ht_resize(generic_ht_t* ht, ht_essentials_t* hte, size_t capacity);
static int ht_plain_insert(generic_ht_t* ht, ht_essentials_t* hte, void* key, void* data);

static inline bool is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

generic_ht_t* ht_create_impl(size_t capacity, ht_essentials_t* hte) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(is_power_of_two(capacity), "capacity is not power of two");

    generic_ht_t* ht = calloc(1, sizeof(generic_ht_t));
    if (ht == NULL) {
        return NULL;
    }
    
    void* slots = calloc(capacity, hte->slot_size);
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
void ht_destroy_impl(generic_ht_t* ht, ht_essentials_t* hte) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(ht != NULL, "ht is null");
    CHECK_INVARIANT_HTE(ht->slots != NULL, "ht slots is null");

    free(ht->slots);
    free(ht);
}

static int ht_resize(generic_ht_t* ht, ht_essentials_t* hte, size_t capacity) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(is_power_of_two(capacity), "capacity is not power of two");

    uint8_t* slots = calloc(capacity, hte->slot_size);
    if (slots == NULL) {
        return -1;
    }

    generic_ht_t new_ht;
    new_ht.slots = slots;
    new_ht.capacity = capacity;
    new_ht.size = 0;
    new_ht.tombstones = 0;

    uint8_t* old_slots = ht->slots;

    for (size_t i = 0; i < ht->capacity; i++) {
        generic_ht_slot_t* slot = (generic_ht_slot_t*)(old_slots + i * hte->slot_size);

        if (slot->state == HTS_OCCUPIED) {
            int res = 
                ht_plain_insert(
                    &new_ht,
                    hte,
                    (void*)slot->key,
                    slot->value);
            
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

int ht_insert_impl(generic_ht_t* ht, ht_essentials_t* hte, void* key, void* data) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(ht != NULL, "ht is null");
    CHECK_INVARIANT_HTE(key != NULL, "key is null");

    if ((double)(ht->size + 1) >= (double)ht->capacity * 0.7) {
        int res = ht_resize(ht, hte, ht->capacity * 2);
        if (res != JK_OK) {
            return res;
        }
    } else if ((double)(ht->tombstones) >= (double)ht->capacity * 0.2) {
        int res = ht_resize(ht, hte, ht->capacity * 2);
        if (res != JK_OK) {
            return res;
        }
    }

    return ht_plain_insert(ht, hte, key, data);
}

int ht_plain_insert(
    generic_ht_t* ht,
    ht_essentials_t* hte,
    void* key, /* NOLINT */
    void* data) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(hte->eq_func != NULL, "hte->eq_func is NULL");
    CHECK_INVARIANT_HTE(hte->hash_func != NULL, "hte->hash_func is NULL");
    
    size_t idx = hte->hash_func(key) & (ht->capacity - 1);
    uint8_t* slots = ht->slots;
    generic_ht_slot_t* insertion_pos = NULL;
    
    for (;;) {
        generic_ht_slot_t* slot = (generic_ht_slot_t*)(slots + idx * hte->slot_size);

        if (slot->state == HTS_EMPTY && insertion_pos == NULL) {
            insertion_pos = slot;
            break;
        }

        if (slot->state == HTS_EMPTY && insertion_pos != NULL) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && hte->eq_func((void*)slot->key, key)) {
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
    memcpy((void*)insertion_pos->key, key, hte->key_size);
    insertion_pos->value = data;

    ht->size += 1;

    return JK_OK;
}

void* ht_lookup_impl(generic_ht_t* ht, ht_essentials_t* hte, void* key) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(hte->eq_func != NULL, "hte->eq_func is NULL");
    CHECK_INVARIANT_HTE(hte->hash_func != NULL, "hte->hash_func is NULL");

    CHECK_INVARIANT_HTE(ht != NULL, "ht is null");
    CHECK_INVARIANT_HTE(key != NULL, "key is null");

    connection_t *result = NULL;
    
    size_t idx = hte->hash_func(key) & (ht->capacity - 1);
    uint8_t* slots = ht->slots;
    
    for (;;) {
        generic_ht_slot_t* slot = (generic_ht_slot_t*)(slots + idx * hte->slot_size);

        if (slot->state == HTS_EMPTY) {
            break;
        }

        if (slot->state == HTS_OCCUPIED && hte->eq_func(&slot->key, key)) {
            result = slot->value;
            break;
        }

        idx = (idx + 1) & (ht->capacity - 1);
    }

    return result;
}

int ht_delete_impl(
    generic_ht_t* ht,
    ht_essentials_t* hte,
    void* key) {
    logger_t* logger = current_logger;

    CHECK_INVARIANT(hte != NULL, "hte is NULL");
    CHECK_INVARIANT_HTE(hte->eq_func != NULL, "hte->eq_func is NULL");
    CHECK_INVARIANT_HTE(hte->hash_func != NULL, "hte->hash_func is NULL");

    CHECK_INVARIANT_HTE(ht != NULL, "ht is null");
    CHECK_INVARIANT_HTE(key != NULL, "key is null");
    
    size_t idx = hte->hash_func(key) & (ht->capacity - 1);
    uint8_t* slots = ht->slots;
    
    for (;;) {
        generic_ht_slot_t* slot = (generic_ht_slot_t*)(slots + idx * hte->slot_size);

        if (slot->state == HTS_EMPTY) {
            return JK_NOT_FOUND;
        }

        if (slot->state == HTS_OCCUPIED && hte->eq_func((void*)slot->key, key)) {
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
