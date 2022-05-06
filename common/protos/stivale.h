#ifndef __PROTOS__STIVALE_H__
#define __PROTOS__STIVALE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <mm/vmm.h>
#include <lib/elf.h>

bool stivale_load(char *config, char *cmdline);

bool stivale_load_by_anchor(void **_anchor, const char *magic,
                            uint8_t *file, uint64_t filesize);
#if port_x86
pagemap_t stivale_build_pagemap(bool level5pg, bool unmap_null, struct elf_range *ranges, size_t ranges_count,
                                bool want_fully_virtual, uint64_t physical_base, uint64_t virtual_base,
                                uint64_t direct_map_offset);
#elif port_aarch64
pagemap_t stivale_build_pagemap(bool unmap_null, struct elf_range *ranges, size_t ranges_count,
                                bool want_fully_virtual, uint64_t physical_base, uint64_t virtual_base,
                                uint64_t direct_map_offset);
#endif

// FIXME: this needs to go into arch/x86
#if port_x86
noreturn void stivale_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stivale_struct, uint64_t stack,
                 bool enable_nx, bool wp, uint32_t local_gdt);
#endif

#endif
