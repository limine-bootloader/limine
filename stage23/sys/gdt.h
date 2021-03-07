#ifndef __SYS__GDT_H__
#define __SYS__GDT_H__

#include <stdint.h>

struct gdtr {
    uint16_t limit;
    uint64_t ptr;
} __attribute__((packed));

struct gdt_desc {
    uint16_t limit;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_hi;
} __attribute__((packed));

extern struct gdtr gdt;

#endif
