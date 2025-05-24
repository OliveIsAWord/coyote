#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../common/str.h"
#include "../common/strmap.h"
#include "../common/vec.h"

#define eprintf(...) (fprintf(stderr, __VA_ARGS__), (void)0)
#define bail(...) (eprintf(__VA_ARGS__), eprintf("\n"), exit(1))

typedef struct {
    // the index of the character following the backslashed newline in `src2`
    size_t index;
    // the byte index of that same character in `src1`
    size_t actual;
} T2Offset;

Vec_typedef(T2Offset);

typedef enum: uint8_t {
    PpTokHeaderName,
    PpTokIdentifier,
    PpTokPpNumber,
    PpTokCharacterConstant,
    PpTokStringLiteral,
    PpTokPunctuator,
    // TODO:
        // - each universal character name that cannot be one of the above
        // - each non-white-space character that cannot be one of the above
    PpTokNewline,
    PpTokWhitespace, // Includes comments
} PpTokKind;

typedef struct {
    string value;
    uint32_t actual_loc;
    uint16_t actual_len;
    PpTokKind kind;
} PpTok;

Vec_typedef(PpTok);

Vec(PpTok) tl3(string src2, Vec(T2Offset) offsets);
void debug_pptoks(Vec(PpTok) toks, string src1);
bool starts_with_cstr(string str, const char *prefix);
bool string_eq_cstr(string str, const char *match);


static void debug_str(string str) {
    eprintf("\"");
    for (size_t i = 0; i < str.len; i++) {
        char c = str.raw[i];
        switch (c) {
            case '\0':
                eprintf("\\0");
                break;
            case '"':
                eprintf("\\\"");
                break;
            case '\\':
                eprintf("\\\\");
                break;
            case '\a':
                eprintf("\\a");
                break;
            case '\b':
                eprintf("\\b");
                break;
            case '\f':
                eprintf("\\f");
                break;
            case '\n':
                eprintf("\\n");
                break;
            case '\r':
                eprintf("\\r");
                break;
            case '\t':
                eprintf("\\t");
                break;
            case '\v':
                eprintf("\\v");
                break;
            case ' ' ... '!':
            case '#' ... '[':
            case ']' ... '~':
                eprintf("%c", c);
                break;
            default:
                eprintf("\\x%02x", c);
        }
    }
    eprintf("\"");
}
