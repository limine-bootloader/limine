asm (
    ".section .entry\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"
    "call main\n\t"
);

#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/print.h>
#include <lib/types.h>

extern symbol bss_begin;
extern symbol bss_end;

void main(int boot_drive) {
    // Zero out .bss section
    for (uint8_t *p = bss_begin; p < bss_end; p++)
        *p = 0;

    init_vga_textmode();
    print("qLoader 2\n\n");

    print("=> Boot drive: %x\n\n", boot_drive);

    for (;;) {
        struct rm_regs r = {0};
        rm_int(0x16, &r, &r);    // Real mode interrupt 16h
        char c = (char)(r.eax & 0xff);
        text_write(&c, 1);
    }
}
