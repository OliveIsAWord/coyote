#include "cobold.h"
#include "tl4_preprocessing.h"

string file_read_to_string(string filepath) {
    // must be null terminated, ugh
    assert(strlen(filepath.raw) == filepath.len);
    FILE *f = fopen(filepath.raw, "r");
    if (!f) {
        bail("Could not open file.");
    }

    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    string contents = string_alloc(len);
    fread(contents.raw, 1, len, f);
    return contents;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        char *my_name = argv[0];
        if (!my_name) {
            my_name = "cobold";
        }
        bail("Usage: %s <FILE>", my_name);
    }
    string filepath = str(argv[1]);

    // TL phase 1: Map source file data to source character set (no-op)
    string src1 = file_read_to_string(filepath);
    //printf("source:\n```\n" str_fmt "```\n", str_arg(src1));
    // we could replace trigraphs here if we cared about supporting them
    // they were removed in C23, so too bad!

    // TL phase 2: remove "\\\n", turning physical lines into logical ones
    string src2 = string_alloc(src1.len);
    src2.len = 0;
    Vec(T2Offset) offsets = vec_new(T2Offset, 8);
    {
        for (size_t i = 0; i < src1.len; i++) {
            char c = src1.raw[i];
            if (c == '\\') {
                if (i >= src1.len - 1) {
                    bail("source file ends in a backslash");
                }
                // TODO: Windows CRLF line endings
                if (src1.raw[i + 1] == '\n') {
                    i += 1;
                    T2Offset offset = { .index = src2.len, .actual = i + 1 };
                    vec_append(&offsets, offset);
                    continue;
                }
            }
            src2.raw[src2.len++] = c;
        }
    }
    if (src1.len != 0 && (src2.len == 0 || src2.raw[src2.len - 1] != '\n')) {
        bail("non-empty source file does not end in a new line character");
    }
    if (false)
    {
        for (size_t i = 0; i < offsets.len; i++) {
            T2Offset o = offsets.at[i];
            eprintf("%lu -> %lu, ", o.index, o.actual);
        }
        eprintf("\n");
    }
    // printf("source:\n```\n" str_fmt "```\n", str_arg(src2));

    Vec(PpTok) src3 = tl3(src2, offsets);
    debug_pptoks(src3, src1);

    Vec(PpTok) src4 = tl4(src3);

    eprintf("input:\n" str_fmt "\noutput:\n", str_arg(src1));
    for (size_t i = 0; i < src4.len; i++) {
        PpTok tok = src4.at[i];
        // if (tok.kind == PpTokWhitespace || tok.kind == PpTokNewline) continue;
        // if (tok.kind == PpTokStringLiteral) {
        //     debug_str(tok.value);
        // } else {
            eprintf(str_fmt, str_arg(tok.value));
        // }
        // eprintf(" ");
    }
    eprintf("\n");
    bail("TODO: compile C code :3");
    return 0;
}
