asm (
    ".section .entry\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"
    "call main\n\t"
);

#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/print.h>

void main(int boot_drive) {
    init_vga_textmode();
    print("qLoader 2\n\n");
    print("=> Boot drive: %x\n", boot_drive);
    print("\n");
    for (;;) {
        struct rm_regs r = {0};
        rm_int(0x16, &r, &r);    // Real mode interrupt 16h
        char c = (char)(r.eax & 0xff);
        text_write(&c, 1);
    }
}
