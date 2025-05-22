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
    string src = file_read_to_string(filepath);
    printf("source:\n```\n" str_fmt "```\n", str_arg(src));
    bail("TODO: compile C code :3");
    return 0;
}
