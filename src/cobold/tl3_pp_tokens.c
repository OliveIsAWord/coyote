#include "tl3_pp_tokens.h"

#define next(str) (assert(str.len > 0), str.len--, str.raw++, (void) 0)

void debug_pptoks(Vec(PpTok) toks, string src1, string src2) {
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
                eprintf("nl    ");
                break;
            case PpTokWhitespace:
                eprintf("ws    ");
                break;
        }
        //eprintf(" %d, %d", tok.loc, tok.len);
        if (src2.len != 0) {
            eprintf(" ");
            string slice = {
                .raw = src2.raw + tok.loc,
                .len = tok.len
            };
            //eprintf("%d", tok.len);
            debug_str(slice);
        }
        if (src1.len != 0 && tok.len != tok.actual_len) {
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

static bool is_identifier_start(char c) {
    // TODO: unicode?
    return is_nondigit(c);
}

static bool is_identifier_continue(char c) {
    // TODO: unicode?
    return is_identifier_start(c) || is_digit(c);
}

static bool starts_with_cstr(string str, const char *prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        if (str.len == 0 || str.raw[i] != prefix[i]) {
            return false;
        }
    }
    return true;
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

// TL phase 3: preprocessing tokenization, including turning comments into whitespace
Vec(PpTok) tl3(string src2, Vec(T2Offset) offsets) {
    Vec(PpTok) src3 = vec_new(PpTok, 8);
    string p = src2;
    while (p.len) {
        char c = p.raw[0];
        PpTokKind kind = -1;
        uint32_t tok_start = p.raw - src2.raw;
        do {
            // newline
            if (c == '\n') {
                next(p);
                kind = PpTokNewline;
                break;
            }
            // whitespace
            if (is_whitespace(c)) {
                do {
                    next(p);
                    c = p.raw[0];
                } while (is_whitespace(c));
                kind = PpTokWhitespace;
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
                break;
            }
            // identifier
            if (is_identifier_start(c)) {
                do {
                    next(p);
                } while (is_identifier_continue(p.raw[0]));
                kind = PpTokIdentifier;
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
                    break;
                }
            }
            debug_pptoks(src3, (string){0}, src2);
            eprintf("unknown preproc tokenization error at ");
            debug_str(p);
            if (kind != (PpTokKind) -1) {
                eprintf("\nlooks like PpTokKind %d forgot to break", kind);
            }
            bail("");
        } while (false);
        assert(kind != (PpTokKind) -1);
        uint32_t tok_end = p.raw - src2.raw;
        uint16_t len = tok_end - tok_start;

        uint32_t actual_start = offset_by(tok_start, offsets);
        uint32_t actual_end = offset_by(tok_end, offsets);
        uint16_t actual_len = actual_end - actual_start;

        //eprintf("= %d, %d, %d; %d, %d, %d\n", c, tok_start, tok_end, real_start, real_end, len);
        PpTok tok = {
            .kind = kind,
            .loc = tok_start,
            .actual_loc = actual_start,
            .len = len,
            .actual_len = actual_len
        };
        vec_append(&src3, tok);
    }
    return src3;
}
