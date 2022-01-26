#include <sys/cpu.h>

extern bool cpuid(uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
extern void outb(uint16_t port, uint8_t value);
extern void outw(uint16_t port, uint16_t value);
extern void outd(uint16_t port, uint32_t value);
extern uint8_t inb(uint16_t port);
extern uint16_t inw(uint16_t port);
extern uint32_t ind(uint16_t port);
extern void mmoutb(uintptr_t addr, uint8_t value);
extern void mmoutw(uintptr_t addr, uint16_t value);
extern void mmoutd(uintptr_t addr, uint32_t value);
extern void mmoutq(uintptr_t addr, uint64_t value);
extern uint8_t mminb(uintptr_t addr);
extern uint16_t mminw(uintptr_t addr);
extern uint32_t mmind(uintptr_t addr);
extern uint64_t mminq(uintptr_t addr);
extern uint64_t rdmsr(uint32_t msr);
extern void wrmsr(uint32_t msr, uint64_t value);
extern uint64_t rdtsc(void);
extern void delay(uint64_t cycles);
