#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/lapic.h>
#include <sys/cpu.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <mm/pmm.h>

struct madt {
    struct sdt header;
    uint32_t local_controller_addr;
    uint32_t flags;
    char     madt_entries_begin[];
} __attribute__((packed));

struct madt_io_apic {
    uint8_t type;
    uint8_t length;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsib;
} __attribute__((packed));

struct dmar {
    struct sdt header;
    uint8_t host_address_width;
    uint8_t flags;
    uint8_t reserved[10];
    symbol  remapping_structures;
} __attribute__((packed));

bool lapic_check(void) {
    uint32_t eax, ebx, ecx, edx;
    if (!cpuid(1, 0, &eax, &ebx, &ecx, &edx))
        return false;

    if (!(edx & (1 << 9)))
        return false;

    return true;
}

uint32_t lapic_read(uint32_t reg) {
    size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
    return mmind(lapic_mmio_base + reg);
}

void lapic_write(uint32_t reg, uint32_t data) {
    size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
    mmoutd(lapic_mmio_base + reg, data);
}

bool x2apic_check(void) {
    uint32_t eax, ebx, ecx, edx;
    if (!cpuid(1, 0, &eax, &ebx, &ecx, &edx))
        return false;

    if (!(ecx & (1 << 21)))
        return false;

    // According to the Intel VT-d spec, we're required
    // to check if bit 0 and 1 of the flags field of the
    // DMAR ACPI table are set, and if they are, we should
    // not report x2APIC capabilities.
    struct dmar *dmar = acpi_get_table("DMAR", 0);
    if (!dmar)
        return true;

    if ((dmar->flags & (1 << 0)) && (dmar->flags & (1 << 1)))
        return false;

    return true;
}

bool x2apic_enable(void) {
    if (!x2apic_check())
        return false;

    uint64_t ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= (1 << 10);
    wrmsr(0x1b, ia32_apic_base);

    return true;
}

uint64_t x2apic_read(uint32_t reg) {
    return rdmsr(0x800 + (reg >> 4));
}

void x2apic_write(uint32_t reg, uint64_t data) {
    wrmsr(0x800 + (reg >> 4), data);
}

static struct madt_io_apic **io_apics = NULL;
static size_t max_io_apics = 0;

void init_io_apics(void) {
    static bool already_inited = false;
    if (already_inited) {
        return;
    }

    struct madt *madt = acpi_get_table("APIC", 0);

    if (madt == NULL) {
        panic("IO APIC error");
    }

    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 1: {
                max_io_apics++;
                continue;
            }
        }
    }

    io_apics = ext_mem_alloc(max_io_apics * sizeof(struct madt_io_apic *));
    max_io_apics = 0;

    // Try to start all APs
    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 1: {
                io_apics[max_io_apics++] = (void *)madt_ptr;
                continue;
            }
        }
    }

    already_inited = true;
}

uint32_t io_apic_read(size_t io_apic, uint32_t reg) {
    uintptr_t base = (uintptr_t)io_apics[io_apic]->address;
    mmoutd(base, reg);
    return mmind(base + 16);
}

void io_apic_write(size_t io_apic, uint32_t reg, uint32_t value) {
    uintptr_t base = (uintptr_t)io_apics[io_apic]->address;
    mmoutd(base, reg);
    mmoutd(base + 16, value);
}

uint32_t io_apic_gsi_count(size_t io_apic) {
	return ((io_apic_read(io_apic, 1) & 0xff0000) >> 16) + 1;
}

void io_apic_mask_all(void) {
    for (size_t i = 0; i < max_io_apics; i++) {
        uint32_t gsi_count = io_apic_gsi_count(i);
        for (uint32_t j = 0; j < gsi_count; j++) {
            uintptr_t ioredtbl = j * 2 + 16;
            io_apic_write(i, ioredtbl, (1 << 16)); // mask
            io_apic_write(i, ioredtbl + 1, 0);
        }
    }
}
