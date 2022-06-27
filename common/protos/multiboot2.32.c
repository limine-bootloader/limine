#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <mm/vmm.h>
#if bios == 1
#  include <sys/idt.h>
#endif
#include <protos/multiboot.h>

noreturn void multiboot2_spinup_32(uint32_t entry_point,
                                   uint32_t multiboot2_info, uint32_t mb_info_target,
                                   uint32_t mb_info_size,
                                   uint32_t elf_ranges, uint32_t elf_ranges_count,
                                   uint32_t slide,
                                   struct mb_reloc_stub *reloc_stub) {
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

    reloc_stub->magic = 0x36d76289;
    reloc_stub->entry_point = entry_point;
    reloc_stub->mb_info_target = mb_info_target;

    asm volatile (
        "jmp *%%ebx"
        :
        : "b"(reloc_stub), "S"(multiboot2_info),
          "c"(mb_info_size), "a"(elf_ranges), "d"(elf_ranges_count),
          "D"(slide)
        : "memory"
    );

    __builtin_unreachable();
}
