#ifndef URUS_CODEGEN_H
#define URUS_CODEGEN_H

#include "ast.h"
#include <stdbool.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int indent;
    int tmp_counter;
} CodeBuf;

void codegen_init(CodeBuf *buf);
void codegen_free(CodeBuf *buf);
void codegen_generate(CodeBuf *buf, AstNode *program);

#endif
