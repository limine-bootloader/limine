#ifndef __SYS__APIC_H__
#define __SYS__APIC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/cpu.h>
#include <lib/blib.h>

#define LAPIC_REG_ICR0     0x300
#define LAPIC_REG_ICR1     0x310
#define LAPIC_REG_SPURIOUS 0x0f0
#define LAPIC_REG_EOI      0x0b0

static inline uint32_t lapic_read(uint32_t reg) {
    size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
    return mmind((void *)(lapic_mmio_base + reg));
}

static inline void lapic_write(uint32_t reg, uint32_t data) {
    size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
    mmoutd((void *)(lapic_mmio_base + reg), data);
}

static inline bool x2apic_check(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    if (!(ecx & (1 << 21)))
        return false;

    return true;
}

static inline bool x2apic_enable(void) {
    if (!x2apic_check())
        return false;

    uint64_t ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= (1 << 10);
    wrmsr(0x1b, ia32_apic_base);

    return true;
}

static inline uint64_t x2apic_read(uint32_t reg) {
    return rdmsr(0x800 + (reg >> 4));
}

static inline void x2apic_write(uint32_t reg, uint64_t data) {
    wrmsr(0x800 + (reg >> 4), data);
}

#endif
