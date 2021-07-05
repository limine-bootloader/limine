#ifndef __PROTOS__STIVALE_H__
#define __PROTOS__STIVALE_H__

#include <stdbool.h>
#include <stdint.h>
#include <sys/e820.h>
#include <mm/vmm.h>

void stivale_load(char *config, char *cmdline);

struct stivale_anchor {
    uint8_t anchor[16];
    uint64_t phys_load_addr;
    uint64_t phys_bss_start;
    uint64_t phys_bss_end;
    uint64_t phys_stivalehdr;
    uint64_t bits;
};

bool stivale_load_by_anchor(struct stivale_anchor **_anchor, const char *magic,
                            uint8_t *file, uint64_t filesize);
pagemap_t stivale_build_pagemap(bool level5pg, bool unmap_null);
__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stivale_struct, uint64_t stack);

#endif
