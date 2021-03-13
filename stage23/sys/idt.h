#ifndef __SYS__IDT_H__
#define __SYS__IDT_H__

#include <stdint.h>

#if defined (bios)

struct idtr {
    uint16_t limit;
    uint32_t ptr;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  unused;
    uint8_t  type_attr;
    uint16_t offset_hi;
} __attribute__((packed));

extern struct idtr idt;

void init_idt(void);

#endif

#endif
