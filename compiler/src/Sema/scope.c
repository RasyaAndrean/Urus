#include "scope.h"
#include <stdlib.h>
#include <string.h>

SemaScope *scope_new(SemaScope *parent) {
    SemaScope *s = calloc(1, sizeof(SemaScope));
    s->parent = parent;
    s->cap = 8;
    s->syms = malloc(sizeof(SemaSymbol) * (size_t)s->cap);
    return s;
}

void scope_free(SemaScope *s) {
    free(s->syms);
    free(s);
}

SemaSymbol *scope_lookup_local(SemaScope *s, const char *name) {
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->syms[i].name, name) == 0) return &s->syms[i];
    }
    return NULL;
}

SemaSymbol *scope_lookup(SemaScope *s, const char *name) {
    for (SemaScope *cur = s; cur; cur = cur->parent) {
        SemaSymbol *sym = scope_lookup_local(cur, name);
        if (sym) return sym;
    }
    return NULL;
}

SemaSymbol *scope_add(SemaScope *s, const char *name, Token tok) {
    if (s->count >= s->cap) {
        s->cap *= 2;
        s->syms = realloc(s->syms, sizeof(SemaSymbol) * (size_t)s->cap);
    }
    SemaSymbol *sym = &s->syms[s->count++];
    memset(sym, 0, sizeof(SemaSymbol));
    sym->name = (char *)name;
    sym->tok = tok;
    return sym;
}
