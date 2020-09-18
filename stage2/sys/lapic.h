#ifndef __DRIVERS__APIC_H__
#define __DRIVERS__APIC_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/cio.h>

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

#endif
