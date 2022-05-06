#if port_x86 == 1
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <mm/vmm.h>
#if bios == 1
#  include <sys/idt.h>
#endif

noreturn void multiboot1_spinup_32(uint32_t entry_point, uint32_t multiboot1_info) {
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
        : "a" (0x2badb002),
          "b" (multiboot1_info),
          "r" (entry_point)
        : "memory"
    );

    __builtin_unreachable();
}
#endif