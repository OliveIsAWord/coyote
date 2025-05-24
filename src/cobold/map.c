#include "map.h"
#include <stdbit.h>

typedef uint64_t u64;

#define OCCUPIED_BIT ((u64) 1 << 63)

u64 rotate_left(u64 x, uint8_t n) {
    return (x << n) | (x >> (64 - n));
}

// FxHasher impl
// https://nnethercote.github.io/2021/12/08/a-brutally-effective-hash-function-in-rust.html
u64 hash(string s) {
    u64 state = 0;
    for (size_t i = 0; i < s.len; i++) {
        char c = s.raw[i];
        state = rotate_left(state, 5);
        state ^= c;
        state *= 0x517c'c1b7'2722'0a95;
    }
    return state;
}

Map map_new() {
    return map_with_capacity(8);
}

Map map_with_capacity(size_t cap) {
    cap = stdc_bit_ceil(cap);
    Vec(Entry) entries = vec_new(Entry, cap);
    assert(entries.cap == cap);
    for (size_t i = 0; i < cap; i++) {
        entries.at[i].hash = 1;
    }
    return (Map){ .entries = entries };
}

Macro map_get(const Map *map, string key) {
    u64 h = hash(key) | OCCUPIED_BIT;
    size_t cap = map->entries.cap;
    size_t i = h % cap;
    for (;; i = (i + 1) % cap) {
        assert(i != (i + cap - 1) % cap); // we've looped back (assuming load factor isn't 100%)
        Entry *e = &map->entries.at[i];
        if (e->hash == h) {
            if (!string_eq(e->key, key)) {
                // i just want to see if this ever triggers
                eprintf("wtf, hash collision??\nin map:  ");
                debug_str(e->key);
                eprintf("\ngetting: ");
                debug_str(key);
                bail("\nhash: %lu", h);
            }
            return e->value;
        }
        if (e->hash != 0) {
            return (Macro){ .start = 0, .len = -1 };
        }
    }
}

void map_insert(Map *map, string key, Macro value) {
    u64 h = hash(key) | OCCUPIED_BIT;
    size_t cap = map->entries.cap;
    size_t i = h % cap;
    for (;; i = (i + 1) % cap) {
        assert(i != (i + cap - 1) % cap); // we've looped back (assuming load factor isn't 100%)
        Entry *e = &map->entries.at[i];
        if (!(e->hash & OCCUPIED_BIT)) {
            *e = (Entry){ .hash = h, .key = key, .value = value };
            return;
        }
        if (e->hash == h) {
            if (!string_eq(e->key, key)) {
                // i just want to see if this ever triggers
                eprintf("wtf, hash collision??\nin map:  ");
                debug_str(e->key);
                eprintf("\ngetting: ");
                debug_str(key);
                bail("\nhash: %lu", h);
            }
            bail("TODO: ensure macro compatibility for redefined " str_fmt, str_arg(key));
        }
    }
}

void map_remove(Map *map, string key) {
    bail("TODO: %s", __func__);
}
