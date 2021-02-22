#ifndef __LIB__TRACE_H__
#define __LIB__TRACE_H__

#include <stdint.h>

void trace_init(void);
char *trace_address(size_t *off, size_t addr);
void print_stacktrace(size_t *base_ptr);

#endif
