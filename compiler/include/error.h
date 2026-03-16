#ifndef URUS_ERROR_H
#define URUS_ERROR_H

#include "token.h"

void report_error(const char *filename, Token *t, const char *msg);
void report_warn(const char *filename, Token *t, const char *msg);
void report(const char *filename, const char *fmt, ...);

#endif
