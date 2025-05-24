#pragma once

#include "cobold.h"

typedef struct {
    uint32_t start;
    uint16_t len;
} Macro;

typedef struct {
    uint64_t hash; // high bit set if occupied; hash is 0 if tombstone
    string key;
    Macro value;
} Entry;

Vec_typedef(Entry);

typedef struct {
    // we use Vec a little queerly here: `len` is the number of elements in the
    // map. We access indices [0, cap) which are all initialized.
    Vec(Entry) entries;
} Map;

Map map_new();
Map map_with_capacity(size_t cap);
Macro map_get(const Map *map, string key);
void map_insert(Map *map, string key, Macro value);
void map_remove(Map *map, string key);
