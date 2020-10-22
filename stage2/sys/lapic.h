#ifndef __SYS__APIC_H__
#define __SYS__APIC_H__

#include <stdint.h>
#include <stdbool.h>

#define LAPIC_REG_ICR0     0x300
#define LAPIC_REG_ICR1     0x310
#define LAPIC_REG_SPURIOUS 0x0f0
#define LAPIC_REG_EOI      0x0b0

bool lapic_check(void);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t data);

bool x2apic_check(void);
bool x2apic_enable(void);
uint64_t x2apic_read(uint32_t reg);
void x2apic_write(uint32_t reg, uint64_t data);

#endif
