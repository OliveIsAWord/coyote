#include "cobold.h"

#define next(str) (assert(str.len > 0), str.len--, str.raw++, (void) 0)

void debug_pptoks(Vec(PpTok) toks, string src1) {
    bool print_whitespace = false;
    bool got_newline = true;
    for (size_t i = 0; i < toks.len; i++) {
        PpTok tok = toks.at[i];
        switch (tok.kind) {
            case PpTokHeaderName:
                eprintf("header");
                break;
            case PpTokIdentifier:
                eprintf("ident ");
                break;
            case PpTokPpNumber:
                eprintf("number");
                break;
            case PpTokCharacterConstant:
                eprintf("char");
                break;
            case PpTokStringLiteral:
                eprintf("string");
                break;
            case PpTokPunctuator:
                eprintf("punct ");
                break;
            case PpTokNewline:
                if (!print_whitespace && got_newline) continue;
                eprintf("nl    ");
                break;
            case PpTokWhitespace:
                if (!print_whitespace) continue;
                eprintf("ws    ");
                break;
        }
        got_newline = tok.kind == PpTokNewline;
        //eprintf(" %d, %d", tok.loc, tok.len);
        eprintf(" ");
        debug_str(tok.value);
        if (src1.len != 0 && tok.value.len != tok.actual_len) {
            eprintf(" : ");
            string slice = {
                .raw = src1.raw + tok.actual_loc,
                .len = tok.actual_len
            };
            debug_str(slice);
        }
        eprintf("\n");
    }
}

static bool is_whitespace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\v':
        case '\f':
            return true;
        default:
            return false;
    }
}

static bool is_digit(char c) {
    switch (c) {
        case '0' ... '9':
            return true;
        default:
            return false;
    }
}

static bool is_nondigit(char c) {
    switch (c) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '_':
            return true;
        default:
            return false;
    }
}

static uint8_t hex_digit(char c) {
    switch (c) {
        case '0' ... '9':
            return c - '0';
        case 'a' ... 'f':
            return c - 'a' + 10;
        case 'A' ... 'F':
            return c - 'A' + 10;
        default:
            return -1;
    }
}

static bool is_identifier_start(char c) {
    // TODO: unicode?
    return is_nondigit(c);
}

static bool is_identifier_continue(char c) {
    // TODO: unicode?
    return is_identifier_start(c) || is_digit(c);
}

bool starts_with_cstr(string str, const char *prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        if (str.len == 0 || str.raw[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

bool string_eq_cstr(string str, const char *match) {
    return starts_with_cstr(str, match) && str.len == strlen(match);
}

static const char *puncts[] = {
    "%:%:",
    "...", "<<=", ">>=",
    "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "::",
    "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",
    "##", "<:", ":>", "<%", "%>", "%:",
    "[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!", "/", "%", "<", ">", "^", "|",
    "?", ":", ";", "=", ",", "#"
};

uint32_t offset_by(uint32_t index, Vec(T2Offset) offsets) {
    T2Offset highest_offset = { .index = 0, .actual = 0 };
    for (size_t i = 0; i < offsets.len; i++) {
        T2Offset offset = offsets.at[i];
        //eprintf("(%d %lu)", index, offset.index);
        if (offset.index > index) break;
        highest_offset = offset;
    }
    return index - highest_offset.index + highest_offset.actual;
}

static string parse_escape(string p) {
     char c = p.raw[0];
     next(p);
     switch (c) {
         case '\'':
         case '"':
         case '?':
         case '\\':
         case 'a':
         case 'b':
         case 'f':
         case 'n':
         case 'r':
         case 't':
         case 'v':
             break;
         case '0' ... '7':
             for (int i = 0; i < 2; i++) {
                 c = p.raw[0];
                 if (!('0' <= c && c <= '7')) break;
                 next(p);
             }
             break;
        case 'x':
             while (true) {
                 c = p.raw[0];
                 if (hex_digit(c) == (uint8_t) -1) break;
                 next(p);
             }
             break;
        default:
             bail("unknown string escape `%c`", c);
     }
     return p;
}

typedef enum: uint8_t {
    HsStart,
    HsHash,
    HsInclude
} HeaderState;

// TL phase 3: preprocessing tokenization, including turning comments into whitespace
Vec(PpTok) tl3(string src2, Vec(T2Offset) offsets) {
    Vec(PpTok) src3 = vec_new(PpTok, 8);
    string p = src2;
    HeaderState hs = HsStart;
    while (p.len) {
        char c = p.raw[0];
        uint32_t tok_start = p.raw - src2.raw;
        PpTokKind kind = -1;
        HeaderState hs_new = -1;
        do {
            // whitespace
            if (is_whitespace(c)) {
                do {
                    next(p);
                    c = p.raw[0];
                } while (is_whitespace(c));
                kind = PpTokWhitespace;
                hs_new = hs;
                break;
            }
            // delimited comment
            if (starts_with_cstr(p, "/*")) {
                next(p);
                next(p);
                while (!starts_with_cstr(p, "*/")) {
                    if (!p.len) {
                        bail("unterminated comment");
                    }
                    next(p);
                }
                next(p);
                next(p);
                kind = PpTokWhitespace;
                hs_new = hs;
                break;
            }
            // line comment
            if (starts_with_cstr(p, "//")) {
                next(p);
                do {
                    next(p);
                    if (p.len == 0) {
                       // NOTE: we catch this in phase 2, so this should not occur.
                       bail("unterminated line comment");
                    }
                } while (p.raw[0] != '\n');
                // NOTE: we do not consume the new line.
                // let the next iteration produce a newline token.
                kind = PpTokWhitespace;
                hs_new = hs;
                break;
            }
            // header name
            if (hs == HsInclude) {
                if (c == '<') {
                    while (true) {
                        next(p);
                        if (starts_with_cstr(p, "//") || starts_with_cstr(p, "/*")) {
                            bail("cannot start comment in header name");
                        }
                        switch (p.raw[0]) {
                            case '\'':
                            case '\\':
                            case '"':
                            case '\n':
                                // Technically this is only illegal in phase 3 if there's actually a
                                // closing `>`. Otherwise, we should stop parsing it as a header name.
                                // But surely it'll be a later parsing error, so oh well!
                                bail("illegal character in header name");
                            default:
                                continue;
                            case '>':
                        }
                        next(p);
                        break;
                    }
                    kind = PpTokHeaderName;
                    hs_new = HsStart;
                    break;
                }
                if (c == '"') {
                    while (true) {
                        next(p);
                        if (starts_with_cstr(p, "//") || starts_with_cstr(p, "/*")) {
                            bail("cannot start comment in header name (yes, even though it's a string)");
                        }
                        switch (p.raw[0]) {
                            case '\'':
                            case '\\':
                            case '\n':
                                // Same caveat here as with `<header>` form.
                                bail("illegal character in header name");
                            default:
                                continue;
                            case '"':
                        }
                        next(p);
                        break;
                    }
                    kind = PpTokHeaderName;
                    hs_new = HsStart;
                    break;
                }
            }
            // newline
            if (c == '\n') {
                next(p);
                kind = PpTokNewline;
                hs_new = HsStart;
                break;
            }
            // character constant
            if (c == '\'') {
                next(p);
                while (true) {
                    c = p.raw[0];
                    next(p);
                    switch (c) {
                        case '\n':
                            bail("newline in character constant");
                        case '\\':
                            p = parse_escape(p);
                        default:
                            continue;
                        case '\'':
                    }
                    break;
                }
                kind = PpTokCharacterConstant;
                hs_new = HsStart;
                break;
            }
            // string literal
            if (c == '"') {
                next(p);
                while (true) {
                    c = p.raw[0];
                    next(p);
                    switch (c) {
                        case '\n':
                            bail("newline in string literal");
                        case '\\':
                            p = parse_escape(p);
                        default:
                            continue;
                        case '"':
                    }
                    break;
                }
                kind = PpTokStringLiteral;
                hs_new = HsStart;
                break;
            }
            // number
            if (c == '.' || is_digit(c)) {
                while (true) {
                    next(p);
                    c = p.raw[0];
                    if (c == '.' || is_identifier_continue(c)) {
                        continue;
                    }
                    if (c == '\'') {
                        c = p.raw[1];
                        if (!is_digit(c) && !is_nondigit(c)) {
                            break;
                        }
                        next(p);
                        continue;
                    }
                    if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
                        c = p.raw[1];
                        if (c != '+' && c != '-') {
                            break;
                        }
                        next(p);
                        continue;
                    }
                    break;
                }
                // TODO: parse suffixes
                kind = PpTokPpNumber;
                hs_new = HsStart;
                break;
            }
            // identifier
            if (is_identifier_start(c)) {
                char *ident_start = p.raw;
                do {
                    next(p);
                } while (is_identifier_continue(p.raw[0]));
                string ident = { .raw = ident_start, .len = p.raw - ident_start };
                kind = PpTokIdentifier;
                hs_new = hs == HsHash && string_eq_cstr(ident, "include") ? HsInclude : HsStart;
                break;
            }
            // punctuator
            {
                const char *matched_punct = NULL;
                for (size_t i = 0; i < sizeof(puncts) / sizeof(puncts[0]); i++) {
                    const char *punct = puncts[i];
                    if (starts_with_cstr(p, punct)) {
                        matched_punct = punct;
                        break;
                    }
                }
                if (matched_punct) {
                    size_t len = strlen(matched_punct);
                    p.raw += len;
                    p.len -= len;
                    kind = PpTokPunctuator;
                    hs_new = strcmp(matched_punct, "#") == 0 ? HsHash : HsStart;
                    break;
                }
            }
            debug_pptoks(src3, (string){0});
            eprintf("unknown preproc tokenization error at ");
            debug_str(p);
            if (kind != (PpTokKind) -1) {
                eprintf("\nlooks like PpTokKind %d forgot to break", kind);
            }
            bail("");
        } while (false);
        assert(kind != (PpTokKind) -1);
        assert(hs_new != (HeaderState) -1);
        hs = hs_new;
        uint32_t tok_end = p.raw - src2.raw;
        uint16_t len = tok_end - tok_start;
        string value_borrow = { .raw = src2.raw + tok_start, .len = len };
        string value = string_clone(value_borrow);

        uint32_t actual_start = offset_by(tok_start, offsets);
        uint32_t actual_end = offset_by(tok_end, offsets);
        uint16_t actual_len = actual_end - actual_start;

        //eprintf("= %d, %d, %d; %d, %d, %d\n", c, tok_start, tok_end, real_start, real_end, len);
        PpTok tok = {
            .kind = kind,
            .value = value,
            .actual_loc = actual_start,
            .actual_len = actual_len
        };
        vec_append(&src3, tok);
    }
    return src3;
}
