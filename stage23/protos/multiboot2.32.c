#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm.h>
#if bios == 1
#  include <sys/idt.h>
#endif

__attribute__((noreturn)) void multiboot2_spinup_32(
                 uint32_t entry_point, uint32_t multiboot2_info) {
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

        "pushl $0x18\n\t"
        "pushl %%edi\n\t"

        "movl $0x36D76289, %%eax\n\t"

        "lret\n\t"
        :
        : "D" (entry_point),
          "b" (multiboot2_info)
        : "memory"
    );

    __builtin_unreachable();
}
