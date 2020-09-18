#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>

#define FLAT_PTR(PTR) (*((int(*)[])(PTR)))

#define BYTE_PTR(PTR)  (*((uint8_t *)(PTR)))
#define WORD_PTR(PTR)  (*((uint16_t *)(PTR)))
#define DWORD_PTR(PTR) (*((uint32_t *)(PTR)))
#define QWORD_PTR(PTR) (*((uint64_t *)(PTR)))

static inline void port_out_b(uint16_t port, uint8_t value) {
    asm volatile ("out dx, al"  : : "a" (value), "d" (port) : "memory");
}

static inline void port_out_w(uint16_t port, uint16_t value) {
    asm volatile ("out dx, ax"  : : "a" (value), "d" (port) : "memory");
}

static inline void port_out_d(uint16_t port, uint32_t value) {
    asm volatile ("out dx, eax" : : "a" (value), "d" (port) : "memory");
}

static inline uint8_t port_in_b(uint16_t port) {
    uint8_t value;
    asm volatile ("in al, dx"  : "=a" (value) : "d" (port) : "memory");
    return value;
}

static inline uint16_t port_in_w(uint16_t port) {
    uint16_t value;
    asm volatile ("in ax, dx"  : "=a" (value) : "d" (port) : "memory");
    return value;
}

static inline uint32_t port_in_d(uint16_t port) {
    uint32_t value;
    asm volatile ("in eax, dx" : "=a" (value) : "d" (port) : "memory");
    return value;
}

static inline void mmoutb(void *addr, uint8_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(BYTE_PTR(addr))
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutw(void *addr, uint16_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(WORD_PTR(addr))
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutd(void *addr, uint32_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(DWORD_PTR(addr))
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutq(void *addr, uint64_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(QWORD_PTR(addr))
        : "r"(value)
        : "memory"
    );
}

static inline uint8_t mminb(void *addr) {
    uint8_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(BYTE_PTR(addr))
        : "memory"
    );
    return ret;
}

static inline uint16_t mminw(void *addr) {
    uint16_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(WORD_PTR(addr))
        : "memory"
    );
    return ret;
}

static inline uint32_t mmind(void *addr) {
    uint32_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(DWORD_PTR(addr))
        : "memory"
    );
    return ret;
}

static inline uint64_t mminq(void *addr) {
    uint64_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(QWORD_PTR(addr))
        : "memory"
    );
    return ret;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx, eax;
    asm volatile ("rdmsr"
                  : "=a" (eax), "=d" (edx)
                  : "c" (msr)
                  : "memory");
    return ((uint64_t)edx << 32) | eax;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("wrmsr"
                  :
                  : "a" (eax), "d" (edx), "c" (msr)
                  : "memory");
}

#define write_cr(reg, val) ({ \
    asm volatile ("mov cr" reg ", %0" : : "r" (val)); \
})

#define read_cr(reg) ({ \
    size_t cr; \
    asm volatile ("mov %0, cr" reg : "=r" (cr)); \
    cr; \
})

#define locked_read(var) ({ \
    typeof(*var) ret = 0; \
    asm volatile ( \
        "lock xadd %1, %0;" \
        : "+r" (ret) \
        : "m" (*(var)) \
        : "memory", "cc" \
    ); \
    ret; \
})

#define locked_write(var, val) ({ \
    typeof(*var) ret = val; \
    asm volatile ( \
        "lock xchg %1, %0;" \
        : "+r" ((ret)) \
        : "m" (*(var)) \
        : "memory" \
    ); \
    ret; \
})

#endif
