#ifndef __SYS__INTERRUPT_H__
#define __SYS__INTERRUPT_H__

#include <stdint.h>
#include <stddef.h>

extern volatile uint64_t global_pit_tick;

extern uint8_t rm_pic0_mask;
extern uint8_t rm_pic1_mask;
extern uint8_t pm_pic0_mask;
extern uint8_t pm_pic1_mask;

struct idt_entry_t {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t unused;
    uint8_t type_attr;
    uint16_t offset_hi;
} __attribute__((packed));

struct idt_ptr_t {
    uint16_t size;
    uint32_t address;
} __attribute__((packed));

void init_idt(void);
void register_interrupt_handler(size_t, void *, uint8_t);

#endif
