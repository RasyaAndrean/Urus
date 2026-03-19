#include "sema.h"
#include "./scope.h"
#include <stdlib.h>
#include <stdarg.h>

static void add_builtin(SemaScope *global, const char *name, AstType *ret,
                        int nparams, ...) {
    SemaSymbol *s = scope_add(global, name, (Token){0});
    s->is_fn = true;
    s->is_builtin = true;
    s->param_count = nparams;
    s->return_type = ret;
    if (nparams > 0) {
        s->params = calloc((size_t)nparams, sizeof(Param));
        va_list args;
        va_start(args, nparams);
        for (int i = 0; i < nparams; i++) {
            s->params[i].name = va_arg(args, char *);
            s->params[i].type = va_arg(args, AstType *);
        }
        va_end(args);
    }
}

#define T_INT   ast_type_simple(TYPE_INT)
#define T_FLOAT ast_type_simple(TYPE_FLOAT)
#define T_BOOL  ast_type_simple(TYPE_BOOL)
#define T_STR   ast_type_simple(TYPE_STR)
#define T_VOID  ast_type_simple(TYPE_VOID)
#define T_ANY   NULL

// Register builtin definition into sema scope
// to prevent unnecessary errors
void sema_register_builtins(SemaScope *global) {
    add_builtin(global, "print",    T_VOID,  1, "value", T_ANY);
    add_builtin(global, "to_str",   T_STR,   1, "value", T_ANY);
    add_builtin(global, "to_int",   T_INT,   1, "value", T_ANY);
    add_builtin(global, "to_float", T_FLOAT, 1, "value", T_ANY);
    add_builtin(global, "len",      T_INT,   1, "arr",   T_ANY);
    add_builtin(global, "push",     T_VOID,  2, "arr", T_ANY, "value", T_ANY);
    add_builtin(global, "pop",      T_VOID,  1, "arr", T_ANY);

    add_builtin(global, "str_len",        T_INT,  1, "s", T_STR);
    add_builtin(global, "str_slice",      T_STR,  3, "s", T_STR, "start", T_INT, "end", T_INT);
    add_builtin(global, "str_find",       T_INT,  2, "s", T_STR, "sub", T_STR);
    add_builtin(global, "str_contains",   T_BOOL, 2, "s", T_STR, "sub", T_STR);
    add_builtin(global, "str_upper",      T_STR,  1, "s", T_STR);
    add_builtin(global, "str_lower",      T_STR,  1, "s", T_STR);
    add_builtin(global, "str_trim",       T_STR,  1, "s", T_STR);
    add_builtin(global, "str_replace",    T_STR,  3, "s", T_STR, "old", T_STR, "new", T_STR);
    add_builtin(global, "str_starts_with",T_BOOL, 2, "s", T_STR, "prefix", T_STR);
    add_builtin(global, "str_ends_with",  T_BOOL, 2, "s", T_STR, "suffix", T_STR);
    add_builtin(global, "str_split",      ast_type_array(T_STR), 2, "s", T_STR, "delim", T_STR);
    add_builtin(global, "char_at",        T_STR,  2, "s", T_STR, "i", T_INT);

    add_builtin(global, "abs",  T_INT,   1, "x", T_INT);
    add_builtin(global, "fabs", T_FLOAT, 1, "x", T_FLOAT);
    add_builtin(global, "sqrt", T_FLOAT, 1, "x", T_FLOAT);
    add_builtin(global, "pow",  T_FLOAT, 2, "x", T_FLOAT, "y", T_FLOAT);
    add_builtin(global, "min",  T_INT,   2, "a", T_INT, "b", T_INT);
    add_builtin(global, "max",  T_INT,   2, "a", T_INT, "b", T_INT);
    add_builtin(global, "fmin", T_FLOAT, 2, "a", T_FLOAT, "b", T_FLOAT);
    add_builtin(global, "fmax", T_FLOAT, 2, "a", T_FLOAT, "b", T_FLOAT);

    add_builtin(global, "input",       T_STR,  0);
    add_builtin(global, "read_file",   T_STR,  1, "path", T_STR);
    add_builtin(global, "write_file",  T_VOID, 2, "path", T_STR, "content", T_STR);
    add_builtin(global, "append_file", T_VOID, 2, "path", T_STR, "content", T_STR);

    add_builtin(global, "exit",   T_VOID, 1, "code", T_INT);
    add_builtin(global, "assert", T_VOID, 2, "cond", T_BOOL, "msg", T_STR);

    add_builtin(global, "is_ok",  T_BOOL, 1, "r", T_ANY);
    add_builtin(global, "is_err", T_BOOL, 1, "r", T_ANY);
    add_builtin(global, "unwrap", T_ANY,  1, "r", T_ANY);
    add_builtin(global, "unwrap_err", T_ANY, 1, "r", T_ANY);
}
