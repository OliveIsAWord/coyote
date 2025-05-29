#include "cobold.h"
#include "tl4_preprocessing.h"
#include "map.h"


typedef struct {
    Vec(PpTok) src4;
    // the tokens from `src3` in reverse order, such that successively popping
    // tokens yields the correct order. Directives that cause macro rescanning
    // push their intermediate output tokens here.
    Vec(PpTok) input_stack;
    Map defines;
} Tl4;

static PpTok peek(const Tl4 *self) {
    Vec(PpTok) *stack = &self->input_stack;
    assert(stack->len);
    return stack->at[stack->len - 1];
}

static PpTok next(Tl4 *self) {
    Vec(PpTok) *stack = &self->input_stack;
    assert(stack->len);
    return vec_pop(stack);
}

static void push(Tl4 *self, PpTok tok) {
    vec_append(&self->src4, tok);
}

/*
static void extend(Vec(PpTok) *dest, Vec(PpTok) toks) {
    for (size_t i = 0; i < toks.len; i++) {
        vec_append(dest, toks.at[i]);
    }
}
*/

static void push_to_input_stack(Tl4 *self, Vec(PpTok) from) {
    for (size_t i = 0; i < from.len; i++) {
        vec_append(&self->input_stack, from.at[from.len - 1 - i]);
    }
}

static void skip_whitespace(Tl4 *self) {
    while (peek(self).kind == PpTokWhitespace) {
        next(self);
    }
}

/*
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
*/

static void macro_replace(Tl4 *self, PpTok tok) {
    Macro m;
    switch (tok.kind) {
        case PpTokHeaderName:
        case PpTokNewline:
            bail("unreachable");
        case PpTokIdentifier:
            m = map_get(&self->defines, tok.value);
            if (m.toks.at) break;
            [[fallthrough]];
        case PpTokWhitespace:
        case PpTokPunctuator:
        case PpTokPpNumber:
        case PpTokCharacterConstant:
        case PpTokStringLiteral:
            push(self, tok);
            return;
    }
    // our `tok` is now an identifier, and `m` is its corresponding macro value
    // TODO: function-like macros
    push_to_input_stack(self, m.toks);
}

static void next_group(Tl4 *self) {
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
            size_t old_len = self->input_stack.len;
            for (; tok.kind != PpTokNewline; tok = next(self)) {
                macro_replace(self, tok);
            }
            assert(self->input_stack.len <= old_len);
            assert(tok.kind == PpTokNewline);
            push(self, tok);
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
        if (next(self).kind != PpTokWhitespace) {
            bail("there must be whitespace after the name of an object-like macro");
        }
        skip_whitespace(self);
        Vec(PpTok) replacement_list = vec_new(PpTok, 8);
        while (true) {
            PpTok tok = next(self);
            if (tok.kind == PpTokNewline) break;
            vec_append(&replacement_list, tok);
        }
        while (replacement_list.at[replacement_list.len - 1].kind == PpTokWhitespace) {
            vec_pop(&replacement_list);
        }
        Macro value = { .toks = replacement_list };
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
    Vec(PpTok) input_stack = vec_new(PpTok, src3.len);
    Tl4 p = {
        .src4 = src4,
        .input_stack = input_stack,
        .defines = map_with_capacity(16)
    };
    push_to_input_stack(&p, src3);
    while (p.input_stack.len) {
        next_group(&p);
    }
    return p.src4;
}
