#ifndef __REAL_H__
#define __REAL_H__

#include <stdint.h>

struct rm_regs {
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
};

void rm_int(uint8_t, struct rm_regs *, struct rm_regs *);

#endif
