#include "token.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void report_error(const char *filename, Token *t, const char *msg) {
    fprintf(stderr, "\x1b[1m%s:%d: \x1b[31mError\x1b[0m: %s\n", filename, t->line, msg);

    FILE *f = fopen(filename, "r");
    if (f) {
        char *buffer = NULL;
        size_t n = 0;
        for (int i = 1; i <= t->line; i++) {
            if (getline(&buffer, &n, f) != -1 && i == t->line) {
                fprintf(stderr, "  %-5d | %s", t->line, buffer);
                fprintf(stderr, "        | ");
                for (int j = 1; j < t->col; j++) fprintf(stderr, " ");
                fprintf(stderr, "\033[1;32m");

                for (size_t k = 0; k < t->length; k++) fprintf(stderr, "^");
                fprintf(stderr, "\033[0m\n");

                free(buffer);
            }
        }
        fclose(f);
    }
}
