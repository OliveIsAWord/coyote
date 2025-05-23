#pragma once

#include "cobold.h"

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
    uint32_t loc;
    uint32_t actual_loc;
    uint16_t len;
    uint16_t actual_len;
    PpTokKind kind;
} PpTok;

Vec_typedef(PpTok);

Vec(PpTok) tl3(string src2, Vec(T2Offset) offsets);
void debug_pptoks(Vec(PpTok) toks, string src1, string src2);
