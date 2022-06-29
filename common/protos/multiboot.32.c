#include <stdint.h>
#include <stdnoreturn.h>
#if bios == 1
#  include <sys/idt.h>
#endif

noreturn void multiboot_spinup_32(
                  uint32_t reloc_stub,
                  uint32_t magic, uint32_t protocol_info,
                  uint32_t entry_point,
                  uint32_t elf_ranges, uint32_t elf_ranges_count) {
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
        "jmp *%%ebx"
        :
        : "b"(reloc_stub), "S"(magic),
          "a"(elf_ranges), "d"(elf_ranges_count),
          "D"(protocol_info), "c"(entry_point)
        : "memory"
    );

    __builtin_unreachable();
}
