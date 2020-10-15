#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/lapic.h>
#include <sys/cpu.h>
#include <lib/blib.h>
#include <lib/acpi.h>

struct dmar {
    struct sdt;
    uint8_t host_address_width;
    uint8_t flags;
    uint8_t reserved[10];
    symbol  remapping_structures;
} __attribute__((packed));

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
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

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
