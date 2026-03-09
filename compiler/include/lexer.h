#ifndef URUS_LEXER_H
#define URUS_LEXER_H

#include "token.h"

typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    int line;
    int line_start;
} Lexer;

void lexer_init(Lexer *l, const char *source, size_t length);
Token lexer_next(Lexer *l);

// Tokenize entire source, returns malloc'd array, sets *count
Token *lexer_tokenize(Lexer *l, int *count);

#endif
