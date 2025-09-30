#include "htt.h"

#include <string.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static size_t connection_hash(const void *vkey) {
    connection_key_t* key = (connection_key_t*)vkey; 

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

static int connection_equal(const void *va, const void *vb) {
    connection_key_t* a = (connection_key_t*)va; 
    connection_key_t* b = (connection_key_t*)vb; 

    if (a->af != b->af) return 0;
    if (a->src_port != b->src_port) return 0;
    if (a->af == 4) {
        return memcmp(&a->src.src_v4, &b->src.src_v4, sizeof(struct in_addr)) == 0;
    } else {
        return memcmp(&a->src.src_v6, &b->src.src_v6, sizeof(struct in6_addr)) == 0;
    }
}

DEFINE_HT(
    connection,
    connection_key_t,
    connection_t,
    connection_equal,
    connection_hash
)