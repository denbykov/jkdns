#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum { HTS_EMPTY, HTS_OCCUPIED, HTS_TOMBSTONE } ht_slot_state_t;

typedef size_t (ht_hash_func)(const void* key);
typedef int (ht_eq_func)(const void* a, const void* b);

typedef struct {
    ht_slot_state_t state;
    void* value;
    uint8_t key[];
} generic_ht_slot_t;

typedef struct {
    void *slots;
    size_t capacity;
    size_t size;
    size_t tombstones;
} generic_ht_t;

typedef struct {
    const char* ht_name;
    size_t slot_size;
    size_t key_size;
    ht_hash_func* hash_func;
    ht_eq_func* eq_func;
} ht_essentials_t;

generic_ht_t* ht_create_impl(size_t capacity, ht_essentials_t* hte);
void ht_destroy_impl(generic_ht_t* ht, ht_essentials_t* hte);

// ToDo: need some strategy to delete values??? Idk
int ht_insert_impl(generic_ht_t* ht, ht_essentials_t* hte, void* key, void* data);
void* ht_lookup_impl(generic_ht_t* ht, ht_essentials_t* hte, void* key);
int ht_delete_impl(generic_ht_t* ht, ht_essentials_t* hte, void* key);

#define DECLARE_HT(name, key_type, value_type) \
    typedef struct { \
        ht_slot_state_t state; \
        value_type* value; /* NOLINT */ \
        key_type key; \
    } name##_ht_slot_t; \
    \
    typedef struct { \
        size_t slot_size; \
        name##_ht_slot_t *slots; \
        size_t capacity; \
        size_t size; \
        size_t tombstones; \
    } name##_ht_t; \
    \
    extern ht_essentials_t name##_ht_essentials; \
    \
    name##_ht_t* name##_ht_create(size_t capacity); \
    void name##_ht_destroy(name##_ht_t* ht); \
    int name##_ht_insert(name##_ht_t* ht, key_type* key, value_type* data); /* NOLINT */ \
    value_type* name##_ht_lookup(name##_ht_t* ht, key_type* key); /* NOLINT */ \
    int name##_ht_delete(name##_ht_t* ht, key_type* key); /* NOLINT */

#define STR1(x) #x
#define STR2(x, y) STR1(x ## y)

#define DEFINE_HT_ESSENTIALS(name, key_type, eq_func_v, hash_func_v) \
ht_essentials_t name##_ht_essentials = { \
    .ht_name = STR2(name, _ht_t), \
    .slot_size = sizeof(name##_ht_slot_t), \
    .key_size = sizeof(key_type), \
    .hash_func = (hash_func_v), \
    .eq_func = (eq_func_v) \
};

#define DEFINE_HT_CREATE(name) \
name##_ht_t* name##_ht_create(size_t capacity) { /* NOLINT */ \
    return (name##_ht_t*)ht_create_impl( \
        capacity, \
        &name##_ht_essentials); \
}

#define DEFINE_HT_DESTROY(name) \
void name##_ht_destroy(name##_ht_t* ht) { /* NOLINT */ \
    return ht_destroy_impl( \
        (generic_ht_t*)ht, \
        &name##_ht_essentials \
    ); \
}

#define DEFINE_HT_INSERT(name, key_type, value_type) \
int name##_ht_insert(name##_ht_t* ht, key_type* key, value_type* data) { /* NOLINT */ \
    return ht_insert_impl( \
        (generic_ht_t*)ht, \
        &name##_ht_essentials, \
        key, \
        data \
    ); \
}

#define DEFINE_HT_LOOKUP(name, key_type, value_type) \
value_type* name##_ht_lookup(name##_ht_t* ht, key_type* key) { /* NOLINT */ \
    return ht_lookup_impl( \
        (generic_ht_t*)ht, \
        &name##_ht_essentials, \
        key \
    ); \
}

#define DEFINE_HT_DELETE(name, key_type) \
int name##_ht_delete(name##_ht_t* ht, key_type* key) { /* NOLINT */ \
    return ht_delete_impl( \
        (generic_ht_t*)ht, \
        &name##_ht_essentials, \
        key \
    ); \
}

#define DEFINE_HT(name, key_type, value_type, eq_func, hash_func) \
    DEFINE_HT_ESSENTIALS(name, key_type, eq_func, hash_func) \
    DEFINE_HT_CREATE(name) \
    DEFINE_HT_DESTROY(name) \
    DEFINE_HT_INSERT(name, key_type, value_type) \
    DEFINE_HT_LOOKUP(name, key_type, value_type) \
    DEFINE_HT_DELETE(name, key_type)
