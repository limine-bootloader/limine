#ifndef __SYS__SMP_H__
#define __SYS__SMP_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm.h>
#define LIMINE_NO_POINTERS
#include <limine.h>

#if defined (__x86_64__) || defined (__i386__)

struct limine_smp_info *init_smp(size_t   *cpu_count,
                                 uint32_t *_bsp_lapic_id,
                                 bool      longmode,
                                 int       paging_mode,
                                 pagemap_t pagemap,
                                 bool      x2apic,
                                 bool      nx,
                                 uint64_t  hhdm,
                                 bool      wp);

#elif defined (__aarch64__)

struct limine_smp_info *init_smp(size_t   *cpu_count,
                                 uint64_t *bsp_mpidr,
                                 pagemap_t pagemap,
                                 uint64_t  mair,
                                 uint64_t  tcr,
                                 uint64_t  sctlr);

#elif defined (__riscv64)

struct limine_smp_info *init_smp(size_t   *cpu_count,
                                 uint64_t bsp_hartid,
                                 pagemap_t pagemap);

#else
#error Unknown architecture
#endif

#endif
