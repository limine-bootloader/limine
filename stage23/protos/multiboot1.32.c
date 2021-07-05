#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm.h>

__attribute__((noreturn)) void multiboot1_spinup_32(
                 uint32_t entry_point, uint32_t multiboot1_info) {
    asm volatile (
        "cld\n\t"

        "pushfl\n\t"
        "pushl $0x18\n\t"
        "pushl %%edi\n\t"

        "movl $0x2BADB002, %%eax\n\t"

        "iretl\n\t"
        :
        : "D" (entry_point),
          "b" (multiboot1_info)
        : "memory"
    );

    __builtin_unreachable();
}
