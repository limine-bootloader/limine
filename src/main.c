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
#include <lib/mbr.h>
#include <fs/echfs.h>

extern symbol bss_begin;
extern symbol bss_end;

#define QWORD_KERNEL "qword.bin"

void main(int boot_drive) {
    // Zero out .bss section
    for (uint8_t *p = bss_begin; p < bss_end; p++)
        *p = 0;

    // Initial prompt.
    init_vga_textmode();
    print("qLoader 2\n\n");
    print("=> Boot drive: %x\n\n", boot_drive);

    // Enumerate partitions.
    struct mbr_part parts[4];
    for (int i = 0; i < 4; i++) {
        print("=> Checking for partition %d...", i);
        int ret = mbr_get_part(&parts[i], boot_drive, i);
        if (ret) {
            print("Not found!\n");
        } else {
            print("Found!\n");
        }
    }

    // Load the file from the chooen partition at 1 MiB.
    int part = 1; // TODO: The boot partition is hardcoded for now.
    print("=> Booting %s in partition %d\n", QWORD_KERNEL, part);
    load_echfs_file(boot_drive, part, (void *)0x100000, QWORD_KERNEL);

    // Boot the kernel.
    asm volatile (
        "jmp 0x100000"
        :
        : "b" ("")
        : "memory"
    );

    /*for (;;) {
        struct rm_regs r = {0};
        rm_int(0x16, &r, &r);    // Real mode interrupt 16h
        char c = (char)(r.eax & 0xff);
        text_write(&c, 1);
    }*/
}
