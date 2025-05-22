#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/str.h"

#define eprintf(...) fprintf (stderr, __VA_ARGS__)
#define bail(...) (eprintf(__VA_ARGS__), eprintf("\n"), exit(1))

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
                    continue;
                }
            }
            src2.raw[src2.len++] = c;
        }
    }
    if (src1.len != 0 && (src2.len == 0 || src2.raw[src2.len - 1] != '\n')) {
        bail("non-empty source file does not end in a new line");
    }
    printf("source:\n```\n" str_fmt "```\n", str_arg(src2));

    bail("TODO: compile C code :3");
    return 0;
}
