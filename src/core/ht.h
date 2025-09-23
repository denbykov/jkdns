#pragma once

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h> // for in_addr/in6_addr, which should be binary compatible with win

typedef struct {
    uint8_t af;
    uint16_t src_port;
    union {
        struct in_addr  src_v4;
        struct in6_addr src_v6;
    } src;
} session_key_t;

typedef enum { HTS_EMPTY, HTS_OCCUPIED, HTS_TOMBSTONE } ht_slot_state_t;

typedef struct {
    session_key_t key;
    ht_slot_state_t state;
    void* value;
} session_ht_slot_t;

typedef struct {
    session_ht_slot_t *slots;
    size_t capacity;
    size_t size;
} session_ht_t;

// capacity should be a power of two
session_ht_t* session_ht_create(size_t capacity);
void session_ht_destroy(session_ht_t* ht);

// ToDo: need some strategy to delete values
int session_ht_insert(session_ht_t* ht, session_key_t* key, void* data);
void* session_ht_lookup(session_ht_t* ht, session_key_t* key);
int session_ht_delete(session_ht_t* ht, session_key_t* key);
