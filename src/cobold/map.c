#include "map.h"
#include "cobold.h"
#include <stdbit.h>

typedef uint64_t u64;

#define OCCUPIED_BIT ((u64) 1 << 63)

u64 rotate_left(u64 x, uint8_t n) {
    return (x << n) | (x >> (64 - n));
}

typedef enum {
    AcceptTombstone,
    SkipTombstone,
} TombstonePersonality;

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
    assert(entries.len == 0);
    assert(entries.cap == cap);
    for (size_t i = 0; i < cap; i++) {
        entries.at[i].hash = 1;
    }
    return (Map){ .entries = entries };
}

static Entry *map_entry_with_hash(const Map *map, string key, u64 h, TombstonePersonality personality) {
    assert(h & OCCUPIED_BIT);
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
            return e;
        }
        if (e->hash == 0) {
            if (personality == AcceptTombstone) {
            return e;
            }
        } else if (!(e->hash & OCCUPIED_BIT)) {
            return e;
        }
    }
}

static Entry *map_entry(const Map *map, string key, TombstonePersonality personality) {
    u64 h = hash(key) | OCCUPIED_BIT;
    return map_entry_with_hash(map, key, h, personality);
}

Macro map_get(const Map *map, string key) {
    Entry *e = map_entry(map, key, SkipTombstone);
    assert(e->hash != 0);
    if (e->hash & OCCUPIED_BIT) {
        return e->value;
    } else {
        return (Macro){ .toks = { .at = NULL, .len = -1 } };
    }
}

void map_insert(Map *map, string key, Macro value) {
    size_t len = map->entries.len;
    size_t cap = map->entries.cap;
    assert(len + 1 < cap);
    if (len >= cap / 2) {
        bail("TODO: resize");
    }
    u64 h = hash(key) | OCCUPIED_BIT;
    Entry *e = map_entry_with_hash(map, key, h, AcceptTombstone);
    if (e->hash & OCCUPIED_BIT) {
        bail("TODO: ensure macro compatibility for redefined " str_fmt, str_arg(key));
    } else {
        *e = (Entry){ .hash = h, .key = key, .value = value };
        map->entries.len++;
        return;
    }
}

void map_remove(Map *map, string key) {
    Entry *e = map_entry(map, key, SkipTombstone);
    assert(e->hash != 0);
    if (e->hash & OCCUPIED_BIT) {
        e->hash = 0;
    }
}

void debug_map(const Map *map) {
    eprintf("================== len: %lu\n", map->entries.len);
    for (size_t i = 0; i < map->entries.cap; i++) {
        Entry e = map->entries.at[i];
        eprintf("%lu %016lx ", i, e.hash);
        if (e.hash & OCCUPIED_BIT) {
            debug_str(e.key);
            eprintf(" ->");
            TokSlice toks = e.value.toks;
            for (size_t i = 0; i < toks.len; i++) {
                PpTok t = toks.at[i];
                if (t.kind == PpTokWhitespace) continue;
                eprintf(" ");
                debug_str(t.value);
            }
        }
        eprintf("\n");
    }
}
