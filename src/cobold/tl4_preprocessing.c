#include "cobold.h"
#include "tl4_preprocessing.h"

typedef struct {
    Vec(PpTok) src4;
    const Vec(PpTok) src3;
    size_t i;
} Tl4;

typedef struct {
    size_t i;
} Save;

static PpTok peek(Tl4 *self) {
    assert(self->i < self->src3.len);
    return self->src3.at[self->i];
}

static PpTok next(Tl4 *self) {
    PpTok tok = peek(self);
    self->i++;
    return tok;
}

static void push(Tl4 *self, PpTok tok) {
    vec_append(&self->src4, tok);
}

static Save save(Tl4 *self) {
    return (Save){ .i = self->i };
}

static void load(Tl4 *self, Save saved) {
    self->i = saved.i;
}

static void skip_whitespace(Tl4 *self) {
    while (peek(self).kind == PpTokWhitespace) {
        next(self);
    }
}

static void expand_ident(Tl4 *self, PpTok ident) {
    assert(ident.kind == PpTokIdentifier);
    // TODO: actual macro replacement
    push(self, ident);
}

static void next_text_line(Tl4 *self) {
    while (true) {
        PpTok tok = next(self);
        switch (tok.kind) {
            case PpTokHeaderName:
                bail("unreachable");
            case PpTokWhitespace:
            case PpTokPpNumber:
            case PpTokCharacterConstant:
            case PpTokStringLiteral:
            case PpTokPunctuator:
                push(self, tok);
                break;
            case PpTokNewline:
                push(self, tok);
                return;
            case PpTokIdentifier:
                expand_ident(self, tok);
        }
    }
}

static void next_group(Tl4 *self) {
    Save saved = save(self);
    PpTok tok = next(self);
    switch (tok.kind) {
        case PpTokHeaderName:
            bail("unreachable");
        case PpTokWhitespace:
            push(self, tok);
            return;
        case PpTokPunctuator:
            if (string_eq_cstr(tok.value, "#")) break;
            [[fallthrough]];
        case PpTokPpNumber:
        case PpTokCharacterConstant:
        case PpTokStringLiteral:
        case PpTokIdentifier:
        case PpTokNewline:
            load(self, saved);
            next_text_line(self);
    }
    // handle # directives
    skip_whitespace(self);
    tok = next(self);
    switch (tok.kind) {
        case PpTokHeaderName:
        case PpTokWhitespace:
            bail("unreachable");
        case PpTokNewline:
            return; // null directive
        case PpTokPunctuator:
        case PpTokPpNumber:
        case PpTokCharacterConstant:
        case PpTokStringLiteral:
            bail("expected identifier after #");
        case PpTokIdentifier:
    }
    if (string_eq_cstr(tok.value, "if")) {
        bail("TODO: #if");
    }
    if (string_eq_cstr(tok.value, "ifdef")) {
        bail("TODO: #ifdef");
    }
    if (string_eq_cstr(tok.value, "ifndef")) {
        bail("TODO: #ifndef");
    }
    // else, elif[[n]def]
    {
        typedef enum {
            Else,
            Elif,
            ElifDef,
            ElifNDef,
            ELSE_KIND_MAX
        } ElseKind;
        static const char *keywords[] = {
            [Else] = "else",
            [Elif] = "elif",
            [ElifDef] = "elifdef",
            [ElifNDef] = "elifndef"
        };
        ElseKind e = -1;
        for (size_t i = 0; i < ELSE_KIND_MAX; i++) {
            if (string_eq_cstr(tok.value, keywords[i])) {
                e = i;
                break;
            }
        }
        if (e != (ElseKind) -1) {
            bail("TODO: #%s", keywords[e]);
        }
    }
    if (string_eq_cstr(tok.value, "endif")) {
        bail("TODO: #endif");
    }
    if (string_eq_cstr(tok.value, "define")) {
        skip_whitespace(self);
        tok = next(self);
        if (tok.kind != PpTokIdentifier) {
            bail("expected macro name");
        }
        string name = tok.value;
        bail("TODO: #define " str_fmt, str_arg(name));
    }
    bail("unknown directive `" str_fmt "`", str_arg(tok.value));
}

Vec(PpTok) tl4(Vec(PpTok) src3) {
    Vec(PpTok) src4 = vec_new(PpTok, 8);
    Tl4 p = { .src3 = src3, .src4 = src4, .i = 0 };
    while (p.i < src3.len) {
        next_group(&p);
    }
    return p.src4;
}
