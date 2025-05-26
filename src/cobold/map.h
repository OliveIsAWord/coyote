#pragma once

#include "cobold.h"

typedef struct {
    PpTok *at;
    size_t len;
} TokSlice;

typedef struct {
    TokSlice toks;
} Macro;

typedef struct {
    uint64_t hash; // high bit set if occupied; hash is 0 if tombstone
    string key;
    Macro value;
} Entry;

Vec_typedef(Entry);

typedef struct {
    // We use Vec a little queerly here: `len` is the
    // number of elements in the map, including tombstones.
    // We access indices [0, cap) which all have initialized `hash` fields.
    Vec(Entry) entries;
} Map;

Map map_new();
Map map_with_capacity(size_t cap);
Macro map_get(const Map *map, string key);
void map_insert(Map *map, string key, Macro value);
void map_remove(Map *map, string key);
void debug_map(const Map *map);
