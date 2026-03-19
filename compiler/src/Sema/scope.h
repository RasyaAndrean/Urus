#ifndef URUS_SEMA_SCOPE_H
#define URUS_SEMA_SCOPE_H

#include "sema.h"

// TODO: Add simple documentation about function here

SemaScope *scope_new(SemaScope *parent);
void scope_free(SemaScope *s);
SemaSymbol *scope_lookup_local(SemaScope *s, const char *name);
SemaSymbol *scope_lookup(SemaScope *s, const char *name);
SemaSymbol *scope_add(SemaScope *s, const char *name, Token tok);

#endif
