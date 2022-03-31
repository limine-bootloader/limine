#ifndef __SYS__SMP_H__
#define __SYS__SMP_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm.h>

struct smp_information {
    uint32_t acpi_processor_uid;
    uint32_t lapic_id;
    uint64_t stack_addr;
    uint64_t goto_address;
    uint64_t extra_argument;
} __attribute__((packed));

struct smp_information *init_smp(size_t    header_hack_size,
                                 void    **header_ptr,
                                 size_t   *cpu_count,
                                 uint32_t *_bsp_lapic_id,
                                 bool      longmode,
                                 bool      lv5,
                                 pagemap_t pagemap,
                                 bool      x2apic,
                                 bool      nx,
                                 uint64_t  hhdm,
                                 bool      wp);

#endif
