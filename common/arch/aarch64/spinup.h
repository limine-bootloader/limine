#ifndef __ARCH__AARCH64__SPINUP_H__
#define __ARCH__AARCH64__SPINUP_H__

#include <lib/blib.h>
#include <mm/vmm.h>

noreturn void c(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stivale_struct, uint64_t stack,
                 bool enable_nx, bool wp);

#endif