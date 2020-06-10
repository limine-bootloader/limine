#include <stddef.h>
#include <stdint.h>
#include <lib/config.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>
#include <fs/file.h>

__attribute__((used))
__attribute__((naked)) static void trampoline(void *p, size_t sz, int boot_drive) {
    asm (
        "tos_trampoline_begin:\n\t"

        "mov edx, dword ptr ss:[esp+12]\n\t"

        "mov ecx, dword ptr ss:[esp+8]\n\t"
        "mov esi, dword ptr ss:[esp+4]\n\t"
        "mov edi, 0x7c00\n\t"
        "cld\n\t"
        "rep movsb\n\t"

        "lgdt [4f - tos_trampoline_begin + 0x500]\n\t"

        "jmp 0x08:1f - tos_trampoline_begin + 0x500\n\t"
        "1: .code16\n\t"
        "mov ax, 0x10\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "mov eax, cr0\n\t"
        "and al, 0xfe\n\t"
        "mov cr0, eax\n\t"
        "jmp 0:2f - tos_trampoline_begin + 0x500\n\t"
        "2:\n\t"
        "xor ax, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"

        "mov ax, 0x96c0\n\t"
        "mov ds, ax\n\t"
        "mov ss, ax\n\t"

        "mov ax, 0x35e0\n\t"
        "mov es, ax\n\t"

        "mov eax, 3\n\t"

        "xor ebx, ebx\n\t"
        "xor ecx, ecx\n\t"
        "mov esi, 0x92\n\t"
        "mov edi, 0x200\n\t"
        "xor ebp, ebp\n\t"

        "mov esp, 0x400\n\t"

        "push 0x246\n\t"
        "push 0x07c0\n\t"
        "push 0x0\n\t"
        "iret\n\t"

        ".code32\n\t"

        "3:\n\t"
        ".quad 0\n\t"
        ".quad 0x00009A000000FFFF\n\t"
        ".quad 0x000092000000FFFF\n\t"
        "4:\n\t"
        ".short 4b - 3b - 1\n\t"
        ".long 3b - tos_trampoline_begin + 0x500\n\t"

        "tos_trampoline_end:\n\t"
    );
    (void)p; (void)sz; (void)boot_drive;
}

extern symbol tos_trampoline_begin;
extern symbol tos_trampoline_end;

void templeos_load(int boot_drive) {
    int kernel_drive; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "DRIVE")) {
            kernel_drive = boot_drive;
        } else {
            kernel_drive = (int)strtoui(buf);
        }
    }

    int kernel_part; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "PARTITION")) {
            panic("PARTITION not specified");
        } else {
            kernel_part = (int)strtoui(buf);
        }
    }

    struct file_handle f;
    if (fopen(&f, kernel_drive, kernel_part, "/Kernel.BIN.C")) {
        panic("TempleOS kernel not found.");
    }

    print("TempleOS kernel size: %U bytes\n", f.size);

    void *kernel = balloc(f.size);
    fread(&f, kernel, 0, f.size);

    void (*t)(void *p, size_t sz, int boot_drive);
    t = (void *)0x500;

    memcpy(t, tos_trampoline_begin, (size_t)tos_trampoline_end - (size_t)tos_trampoline_begin);

    t(kernel, f.size, kernel_drive);

    for (;;);
}
