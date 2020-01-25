#ifndef __SYS__INTERRUPT_H__
#define __SYS__INTERRUPT_H__

#include <stdint.h>
#include <stddef.h>

extern volatile uint64_t global_pit_tick;
extern volatile int kbd_int;

extern uint8_t rm_pic0_mask;
extern uint8_t rm_pic1_mask;
extern uint8_t pm_pic0_mask;
extern uint8_t pm_pic1_mask;

void init_idt(void);
void register_interrupt_handler(size_t, void *, uint8_t);

#endif
