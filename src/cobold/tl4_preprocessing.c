#include "cobold.h"
#include "tl4_preprocessing.h"
#include "map.h"

typedef struct {
    Vec(PpTok) src4;
    const Vec(PpTok) src3;
    Map defines;
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

static void extend(Vec(PpTok) *dest, Vec(PpTok) toks) {
    for (size_t i = 0; i < toks.len; i++) {
        vec_append(dest, toks.at[i]);
    }
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
}

static Vec(PpTok) macro_replace(Tl4 *self, TokSlice toks) {
    Vec(PpTok) expanded = vec_new(PpTok, 8);
    for (size_t i = 0; i < toks.len; i++) {
        PpTok tok = toks.at[i];
        Macro m;
        switch (tok.kind) {
            case PpTokHeaderName:
                bail("unreachable");
            case PpTokIdentifier:
                m = map_get(&self->defines, tok.value);
                if (m.toks.at) break;
                [[fallthrough]];
            case PpTokWhitespace:
            case PpTokPpNumber:
            case PpTokCharacterConstant:
            case PpTokStringLiteral:
            case PpTokPunctuator:
            case PpTokNewline:
                vec_append(&expanded, tok);
                continue;
        }
        Vec(PpTok) expanded_macro = macro_replace(self, m.toks);
        extend(&expanded, expanded_macro);
    }
    return expanded;
}

static void next_group(Tl4 *self) {
    size_t text_line_start = self->i;
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
            while (tok.kind != PpTokNewline) {
                tok = next(self);
            }
            TokSlice slice = {
                .at = self->src3.at + text_line_start,
                .len = self->i - text_line_start
            };
            Vec(PpTok) toks = macro_replace(self, slice);
            extend(&self->src4, toks);
            return;
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
        // lparen following macro name without space
        if (peek(self).kind == PpTokPunctuator && string_eq_cstr(peek(self).value, "(")) {
            bail("TODO: function-like macros");
        }
        skip_whitespace(self);
        size_t i_start = self->i;
        size_t i_end = i_start;
        while (true) {
            PpTokKind tok = next(self).kind;
            if (tok == PpTokNewline) break;
            if (tok != PpTokWhitespace) i_end = self->i;
        }
        Macro value = { .toks = {
            .at = &self->src3.at[i_start],
            .len = i_end - i_start,
        } };
        map_insert(&self->defines, name, value);
        debug_map(&self->defines);
        return;
    }
    if (string_eq_cstr(tok.value, "undef")) {
        skip_whitespace(self);
        tok = next(self);
        if (tok.kind != PpTokIdentifier) {
            bail("expected macro name");
        }
        string name = tok.value;
        skip_whitespace(self);
        if (next(self).kind != PpTokNewline) {
            bail("expected end of #undef");
        }
        map_remove(&self->defines, name);
        debug_map(&self->defines);
        return;
    }
    bail("unknown directive `" str_fmt "`", str_arg(tok.value));
}

Vec(PpTok) tl4(Vec(PpTok) src3) {
    Vec(PpTok) src4 = vec_new(PpTok, 8);
    Tl4 p = {
        .src3 = src3,
        .src4 = src4,
        .defines = map_with_capacity(16),
        .i = 0
    };
    while (p.i < src3.len) {
        next_group(&p);
    }
    return p.src4;
}
