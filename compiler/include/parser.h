#ifndef URUS_PARSER_H
#define URUS_PARSER_H

#include "ast.h"
#include "token.h"
#include "lexer.h"

typedef struct {
    Token *tokens;
    const char *filename;
    int count;
    int pos;
    bool had_error;
} Parser;

void parser_init(Parser *p, Token *tokens, int count);
AstNode *parser_parse(Parser *p);

#endif
