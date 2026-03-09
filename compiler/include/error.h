#ifndef SEMA_ERROR_H
#define SEMA_ERROR_H

#include "token.h"

void report_error(const char *filename, Token *t, const char *msg);

#endif
