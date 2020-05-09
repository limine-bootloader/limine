#ifndef __LIB__PRINT_H__
#define __LIB__PRINT_H__

#include <stdarg.h>

void print(const char *fmt, ...);
void vprint(const char *fmt, va_list args);

#endif
