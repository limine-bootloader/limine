#pragma once

#include <stdarg.h>
#include <stddef.h>

extern void (*stivale2_print)(const char *buf, size_t size);

void e9_putc(char c);
void e9_print(const char *msg);
void e9_puts(const char *msg);
void e9_printf(const char *format, ...);
