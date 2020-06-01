#ifndef __LIB__REAL_H__
#define __LIB__REAL_H__

#include <stdint.h>

#define rm_seg(x) ((uint16_t)(((int)x & 0xffff0) >> 4))
#define rm_off(x) ((uint16_t)(((int)x & 0x0000f) >> 0))

#define rm_desegment(seg, off) (((uint32_t)(seg) << 4) + (uint32_t)(off))

#define EFLAGS_CF (1 << 0)
#define EFLAGS_ZF (1 << 6)

struct rm_regs {
    uint32_t eflags;
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
};

void rm_int(uint8_t int_no, struct rm_regs *out_regs, struct rm_regs *in_regs);

void rm_flush_irqs(void);

#endif
