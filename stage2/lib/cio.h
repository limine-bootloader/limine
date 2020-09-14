#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>
#include <lib/asm.h>

static inline void port_out_b(uint16_t port, uint8_t value) {
    ASM("out dx, al\n\t",  : "a" (value), "d" (port) : "memory");
}

static inline void port_out_w(uint16_t port, uint16_t value) {
    ASM("out dx, ax\n\t",  : "a" (value), "d" (port) : "memory");
}

static inline void port_out_d(uint16_t port, uint32_t value) {
    ASM("out dx, eax\n\t", : "a" (value), "d" (port) : "memory");
}

static inline uint8_t port_in_b(uint16_t port) {
    uint8_t value;
    ASM("in al, dx\n\t",  "=a" (value) : "d" (port) : "memory");
    return value;
}

static inline uint16_t port_in_w(uint16_t port) {
    uint16_t value;
    ASM("in ax, dx\n\t",  "=a" (value) : "d" (port) : "memory");
    return value;
}

static inline uint32_t port_in_d(uint16_t port) {
    uint32_t value;
    ASM("in eax, dx\n\t", "=a" (value) : "d" (port) : "memory");
    return value;
}

#endif
