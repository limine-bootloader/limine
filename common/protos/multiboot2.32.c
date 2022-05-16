#if defined (__i386__) || defined (__x86_64__)
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <mm/vmm.h>
#if bios == 1
#  include <arch/x86/idt.h>
#endif

noreturn void multiboot2_spinup_32(uint32_t entry_point, uint32_t multiboot2_info) {
#if bios == 1
    struct idtr idtr;

    idtr.limit = 0x3ff;
    idtr.ptr = 0;

    asm volatile (
        "lidt %0"
        :
        : "m" (idtr)
        : "memory"
    );
#endif

    asm volatile (
        "cld\n\t"

        "push %2\n\t"

        "xor %%ecx, %%ecx\n\t"
        "xor %%edx, %%edx\n\t"
        "xor %%esi, %%esi\n\t"
        "xor %%edi, %%edi\n\t"
        "xor %%ebp, %%ebp\n\t"

        "ret\n\t"
        :
        : "a" (0x36d76289),
          "b" (multiboot2_info),
          "r" (entry_point)
        : "memory"
    );

    __builtin_unreachable();
}
#endif