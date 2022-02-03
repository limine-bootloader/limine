#include <stdint.h>
#include <stdnoreturn.h>
#include <sys/gdt.h>

noreturn void linux_spinup(void *entry, void *boot_params) {
    struct gdt_desc linux_gdt_descs[4];
    linux_gdt_descs[0] = (struct gdt_desc){0};
    linux_gdt_descs[1] = (struct gdt_desc){0};

    linux_gdt_descs[2] = (struct gdt_desc){
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10011010,
        .granularity = 0b11001111,
        .base_hi     = 0x00
    };

    linux_gdt_descs[3] = (struct gdt_desc){
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10010010,
        .granularity = 0b11001111,
        .base_hi     = 0x00
    };

    struct gdtr linux_gdt;
    linux_gdt.limit = sizeof(linux_gdt_descs) - 1;
    linux_gdt.ptr   = (uintptr_t)linux_gdt_descs;

    asm volatile (
        "lgdt %0\n\t"

        "pushl $0x10\n\t"
        "call 1f\n\t"
        "1:\n\t"
        "addl $2f-1b, (%%esp)\n\t"
        "lret\n\t"
        "2:\n\t"

        "movl $0x18, %%eax\n\t"
        "movl %%eax, %%ds\n\t"
        "movl %%eax, %%es\n\t"
        "movl %%eax, %%fs\n\t"
        "movl %%eax, %%gs\n\t"
        "movl %%eax, %%ss\n\t"

        "xorl %%ebp, %%ebp\n\t"
        "xorl %%edi, %%edi\n\t"
        "xorl %%ebx, %%ebx\n\t"

        "cld\n\t"

        "jmp *%%ecx\n\t"
        :
        : "m"(linux_gdt), "c"(entry), "S"(boot_params)
        : "memory"
    );

    __builtin_unreachable();
}
