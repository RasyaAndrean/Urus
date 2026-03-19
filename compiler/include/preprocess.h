#ifndef URUS_PREPROCESS_H
#define URUS_PREPROCESS_H

#include "ast.h"
#include <stdbool.h>

// --- Import resolution ---

// Resolves and merges all import declarations in the program AST.
bool preprocess_imports(AstNode *program, const char *base_file);

// Still an Idea: reset state between file compilations (if you ever do multi-file
// compilation in one process lifetime)
// void preprocess_reset(void);

#endif
