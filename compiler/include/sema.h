#ifndef URUS_SEMA_H
#define URUS_SEMA_H

#include "ast.h"
#include <stdbool.h>

typedef struct {
    char *name;
    AstType *type;
    Token tok; // For error tracking
    bool is_mut;
    bool is_fn;
    bool is_referenced;
    bool is_builtin; // To avoid making warning on builtin function/variable
    Param *params;
    int param_count;
    AstType *return_type;
    bool is_struct;
    Param *fields;
    int field_count;
    bool is_enum;
    EnumVariant *variants;
    int variant_count;
} SemaSymbol;

typedef struct Scope {
    SemaSymbol *syms;
    int count, cap;
    struct Scope *parent;
} SemaScope;

typedef struct {
    SemaScope *current;
    AstType *current_fn_return;
    const char *filename;
    int errors;
    int loop_depth;
} SemaCtx;

// Returns true if analysis succeeded (no errors)
bool sema_analyze(AstNode *program, const char *filename);

// Builtin registration (sema_builtins.c)
void sema_register_builtins(SemaScope *global);

#endif
