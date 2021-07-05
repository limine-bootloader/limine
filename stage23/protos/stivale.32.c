#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mm/vmm.h>

__attribute__((noreturn)) void stivale_spinup_32(
                 int bits, bool level5pg, uint32_t pagemap_top_lv,
                 uint32_t entry_point_lo, uint32_t entry_point_hi,
                 uint32_t stivale_struct_lo, uint32_t stivale_struct_hi,
                 uint32_t stack_lo, uint32_t stack_hi) {
    uint64_t casted_to_64[] = {
        (uint64_t)stivale_struct_lo | ((uint64_t)stivale_struct_hi << 32),
        (uint64_t)entry_point_lo | ((uint64_t)entry_point_hi << 32),
        (uint64_t)stack_lo | ((uint64_t)stack_hi << 32)
    };

    if (bits == 64) {
        if (level5pg) {
            // Enable CR4.LA57
            asm volatile (
                "movl %%cr4, %%eax\n\t"
                "btsl $12, %%eax\n\t"
                "movl %%eax, %%cr4\n\t"
                ::: "eax", "memory"
            );
        }

        asm volatile (
            "cld\n\t"
            "movl %%eax, %%cr3\n\t"
            "movl %%cr4, %%eax\n\t"
            "btsl $5, %%eax\n\t"
            "movl %%eax, %%cr4\n\t"
            "movl $0xc0000080, %%ecx\n\t"
            "rdmsr\n\t"
            "btsl $8, %%eax\n\t"
            "wrmsr\n\t"
            "movl %%cr0, %%eax\n\t"
            "btsl $31, %%eax\n\t"
            "movl %%eax, %%cr0\n\t"
            "call 1f\n\t"
            "1: popl %%eax\n\t"
            "addl $8, %%eax\n\t"
            "pushl $0x28\n\t"
            "pushl %%eax\n\t"
            "lret\n\t"
            ".code64\n\t"
            "movl $0x30, %%eax\n\t"
            "movl %%eax, %%ds\n\t"
            "movl %%eax, %%es\n\t"
            "movl %%eax, %%fs\n\t"
            "movl %%eax, %%gs\n\t"
            "movl %%eax, %%ss\n\t"

            // Since we don't really know what is now present in the upper
            // 32 bits of the 64 bit registers, clear up the upper bits
            // of the register that points to the 64-bit casted value array.
            "movl %%esi, %%esi\n\t"

            // Move in 64-bit values
            "movq 0x00(%%rsi), %%rdi\n\t"
            "movq 0x08(%%rsi), %%rbx\n\t"
            "movq 0x10(%%rsi), %%rsi\n\t"

            // Let's pretend we push a return address
            "testq %%rsi, %%rsi\n\t"
            "jz 1f\n\t"

            "subq $8, %%rsi\n\t"
            "movq $0, (%%rsi)\n\t"

            "1:\n\t"
            "pushq $0x30\n\t"
            "pushq %%rsi\n\t"
            "pushfq\n\t"
            "pushq $0x28\n\t"
            "pushq %%rbx\n\t"

            "xorl %%eax, %%eax\n\t"
            "xorl %%ebx, %%ebx\n\t"
            "xorl %%ecx, %%ecx\n\t"
            "xorl %%edx, %%edx\n\t"
            "xorl %%esi, %%esi\n\t"
            "xorl %%ebp, %%ebp\n\t"
            "xorq %%r8,  %%r8\n\t"
            "xorq %%r9,  %%r9\n\t"
            "xorq %%r10, %%r10\n\t"
            "xorq %%r11, %%r11\n\t"
            "xorq %%r12, %%r12\n\t"
            "xorq %%r13, %%r13\n\t"
            "xorq %%r14, %%r14\n\t"
            "xorq %%r15, %%r15\n\t"

            "iretq\n\t"
            ".code32\n\t"
            :
            : "a" (pagemap_top_lv), "S" (casted_to_64)
            : "memory"
        );
    } else if (bits == 32) {
        asm volatile (
            "cld\n\t"

            "movl %%esi, %%esp\n\t"
            "pushl %%edi\n\t"
            "pushl $0\n\t"

            "pushfl\n\t"
            "pushl $0x18\n\t"
            "pushl %%ebx\n\t"

            "xorl %%eax, %%eax\n\t"
            "xorl %%ebx, %%ebx\n\t"
            "xorl %%ecx, %%ecx\n\t"
            "xorl %%edx, %%edx\n\t"
            "xorl %%esi, %%esi\n\t"
            "xorl %%edi, %%edi\n\t"
            "xorl %%ebp, %%ebp\n\t"

            "iretl\n\t"
            :
            : "D" ((uint32_t)casted_to_64[0]),
              "b" ((uint32_t)casted_to_64[1]),
              "S" ((uint32_t)casted_to_64[2])
            : "memory"
        );
    }

    __builtin_unreachable();
}
