#include <sys/gdt.h>

__attribute__((noreturn)) void linux_spinup(void *entry, void *boot_params) {
    struct gdt_desc linux_gdt_descs[] = {
        {0},

        {0},

        {
            .limit       = 0xffff,
            .base_low    = 0x0000,
            .base_mid    = 0x00,
            .access      = 0b10011010,
            .granularity = 0b11001111,
            .base_hi     = 0x00
        },

        {
            .limit       = 0xffff,
            .base_low    = 0x0000,
            .base_mid    = 0x00,
            .access      = 0b10010010,
            .granularity = 0b11001111,
            .base_hi     = 0x00
        }
    };

    struct gdtr linux_gdt = {
        sizeof(linux_gdt_descs) - 1,
        (uintptr_t)linux_gdt_descs,
#if defined (bios)
        0
#endif
    };

    // Load invalid IDT
    uint64_t invalid_idt[2] = {0, 0};
    asm volatile (
        "lidt %0"
        :
        : "m" (invalid_idt)
        : "memory"
    );

    asm volatile (
        "lgdt %0\n\t"

        "push 0x10\n\t"
        "call 1f\n\t"
        "1:\n\t"
        "add dword ptr [esp], 5\n\t"
        "retf\n\t"

        "mov eax, 0x18\n\t"
        "mov ds, eax\n\t"
        "mov es, eax\n\t"
        "mov fs, eax\n\t"
        "mov gs, eax\n\t"
        "mov ss, eax\n\t"

        "xor ebp, ebp\n\t"
        "xor edi, edi\n\t"
        "xor ebx, ebx\n\t"

        "jmp ecx\n\t"
        :
        : "m"(linux_gdt), "c"(entry), "S"(boot_params)
        : "memory"
    );

    __builtin_unreachable();
}
