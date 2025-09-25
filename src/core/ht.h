#pragma once

#include <stddef.h>
#include <stdint.h>

#include "connection.h"

typedef address_t connection_key_t;

typedef enum { HTS_EMPTY, HTS_OCCUPIED, HTS_TOMBSTONE } ht_slot_state_t;

typedef struct {
    connection_key_t key;
    ht_slot_state_t state;
    connection_t* value;
} connection_ht_slot_t;

typedef struct {
    connection_ht_slot_t *slots;
    size_t capacity;
    size_t size;
    size_t tombstones;
} connection_ht_t;

// capacity should be a power of two
connection_ht_t* connection_ht_create(size_t capacity);
void connection_ht_destroy(connection_ht_t* ht);

// ToDo: need some strategy to delete values
int connection_ht_insert(connection_ht_t* ht, connection_key_t* key, connection_t* data);
connection_t* connection_ht_lookup(connection_ht_t* ht, connection_key_t* key);
int connection_ht_delete(connection_ht_t* ht, connection_key_t* key);
