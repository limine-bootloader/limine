asm (
    ".section .early_boot\n\t"
    "cli\n\t"
    "jmp 0x0:1f\n\t"
    "1:\n\t"
    "xor ax, ax\n\t"
    "mov ds, ax\n\t"
    "mov es, ax\n\t"
    "mov fs, ax\n\t"
    "mov gs, ax\n\t"
    "mov ss, ax\n\t"
    "mov sp, 0x7c00\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"
    "call main\n\t"
);

void bios_print(const char *str) {
    asm (
        "1:\n\t"
        "lodsb\n\t"
        "test al, al\n\t"
        "jz 2f\n\t"
        "int 0x10\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        :
        : "a"(0x0e00), "S"(str)
        : "cc", "memory"
    );
}

void main(int boot_drive) {
    // TODO
    bios_print("hello world from qloader2");
    for (;;);
}
