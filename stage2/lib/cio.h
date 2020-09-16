#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>

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

#endif
