#ifndef __LIB__SMP_H__
#define __LIB__SMP_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm64.h>

struct smp_information {
    uint32_t acpi_processor_uid;
    uint32_t lapic_id;
    uint64_t stack_addr;
    uint64_t goto_address;
} __attribute__((packed));

struct smp_information *init_smp(size_t   *cpu_count,
                                 bool      longmode,
                                 pagemap_t pagemap,
                                 bool      x2apic);

#endif
