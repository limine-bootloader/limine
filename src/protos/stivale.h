#ifndef __PROTOS__STIVALE_H__
#define __PROTOS__STIVALE_H__

#include <stdbool.h>
#include <stdint.h>

void stivale_load(char *cmdline, int boot_drive);
__attribute__((noreturn)) void stivale_spinup(int bits, bool level5pg,
                 uint64_t entry_point, void *stivale_struct, uint64_t stack);

#endif
