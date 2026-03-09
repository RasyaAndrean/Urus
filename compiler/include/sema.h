#ifndef URUS_SEMA_H
#define URUS_SEMA_H

#include "ast.h"
#include <stdbool.h>

// Returns true if analysis succeeded (no errors)
bool sema_analyze(AstNode *program, const char *filename);

#endif
