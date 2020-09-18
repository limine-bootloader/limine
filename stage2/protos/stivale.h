#ifndef __PROTOS__STIVALE_H__
#define __PROTOS__STIVALE_H__

#include <stdbool.h>
#include <stdint.h>
#include <lib/memmap.h>
#include <sys/e820.h>
#include <mm/vmm64.h>

void stivale_load(char *cmdline, int boot_drive);

pagemap_t stivale_build_pagemap(bool level5pg, struct e820_entry_t *memmap,
                                size_t memmap_entries);
__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t pagemap,
                 uint64_t entry_point, void *stivale_struct, uint64_t stack);

#endif
