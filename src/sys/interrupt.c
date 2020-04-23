#include <stddef.h>
#include <stdint.h>
#include <sys/interrupt.h>
#include <lib/cio.h>
#include <lib/blib.h>
#include <lib/real.h>

__attribute__((interrupt)) static void unhandled_int(void *r) {
    (void)r;
    panic("Unhandled interrupt");
}

volatile uint64_t global_pit_tick = 0;

__attribute__((interrupt)) static void pit_irq(void *r) {
    (void)r;
    global_pit_tick++;
    port_out_b(0x20, 0x20);
}

__attribute__((naked)) static void ivt_timer_isr(void) {
    asm (
        ".code16\n\t"
        "pushf\n\t"
        "push bx\n\t"
        "mov ebx, dword ptr ds:[1f]\n\t"
        "inc dword ptr ds:[ebx]\n\t"
        "pop bx\n\t"
        "popf\n\t"
        "iret\n\t"
        ".code32\n\t"
        "1: .long global_pit_tick\n\t"
    );
}

uint8_t rm_pic0_mask = 0xff;
uint8_t rm_pic1_mask = 0xff;
uint8_t pm_pic0_mask = 0xff;
uint8_t pm_pic1_mask = 0xff;

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

#define IDT_MAX_ENTRIES 16

static struct idt_entry_t idt[IDT_MAX_ENTRIES];

void init_idt(void) {
    rm_pic0_mask = port_in_b(0x21);
    rm_pic1_mask = port_in_b(0xa1);

    for (int i = 0; i < IDT_MAX_ENTRIES; i++) {
        register_interrupt_handler(i, unhandled_int, 0x8e);
    }

    register_interrupt_handler(0x08, pit_irq, 0x8e);

    ivt_register_handler(0x1c, ivt_timer_isr);

    struct idt_ptr_t idt_ptr = {
        sizeof(idt) - 1,
        (uint32_t)idt
    };

    asm volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );

    pm_pic0_mask = 0xfe;
    pm_pic1_mask = 0xff;
    port_out_b(0x21, pm_pic0_mask);
    port_out_b(0xa1, pm_pic1_mask);

    asm volatile ("sti");
}

void register_interrupt_handler(size_t vec, void *handler, uint8_t type) {
    uint32_t p = (uint32_t)handler;

    idt[vec].offset_lo = (uint16_t)p;
    idt[vec].selector = 0x18;
    idt[vec].unused = 0;
    idt[vec].type_attr = type;
    idt[vec].offset_hi = (uint16_t)(p >> 16);
}

void ivt_register_handler(int vect, void *isr) {
    volatile uint32_t *ivt = (volatile void *)0;
    ivt[vect] = rm_seg(isr) << 16 | rm_off(isr);
}
