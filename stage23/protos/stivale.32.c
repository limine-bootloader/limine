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
                "mov eax, cr4\n\t"
                "bts eax, 12\n\t"
                "mov cr4, eax\n\t" ::: "eax", "memory"
            );
        }

        asm volatile (
            "cli\n\t"
            "cld\n\t"
            "mov cr3, eax\n\t"
            "mov eax, cr4\n\t"
            "or eax, 1 << 5\n\t"
            "mov cr4, eax\n\t"
            "mov ecx, 0xc0000080\n\t"
            "rdmsr\n\t"
            "or eax, 1 << 8\n\t"
            "wrmsr\n\t"
            "mov eax, cr0\n\t"
            "or eax, 1 << 31\n\t"
            "mov cr0, eax\n\t"
            "call 1f\n\t"
            "1: pop eax\n\t"
            "add eax, 8\n\t"
            "push 0x28\n\t"
            "push eax\n\t"
            "retf\n\t"
            ".code64\n\t"
            "mov ax, 0x30\n\t"
            "mov ds, ax\n\t"
            "mov es, ax\n\t"
            "mov fs, ax\n\t"
            "mov gs, ax\n\t"
            "mov ss, ax\n\t"

            // Since we don't really know what is now present in the upper
            // 32 bits of the 64 bit registers, clear up the upper bits
            // of the register that points to the 64-bit casted value array.
            "mov esi, esi\n\t"

            // Move in 64-bit values
            "mov rdi, qword ptr [rsi + 0]\n\t"
            "mov rbx, qword ptr [rsi + 8]\n\t"
            "mov rsi, qword ptr [rsi + 16]\n\t"

            // Let's pretend we push a return address
            "test rsi, rsi\n\t"
            "jz 1f\n\t"

            "sub rsi, 8\n\t"
            "mov qword ptr [rsi], 0\n\t"

            "1:\n\t"
            "push 0x30\n\t"
            "push rsi\n\t"
            "pushfq\n\t"
            "push 0x28\n\t"
            "push rbx\n\t"

            "xor rax, rax\n\t"
            "xor rbx, rbx\n\t"
            "xor rcx, rcx\n\t"
            "xor rdx, rdx\n\t"
            "xor rsi, rsi\n\t"
            "xor rbp, rbp\n\t"
            "xor r8,  r8\n\t"
            "xor r9,  r9\n\t"
            "xor r10, r10\n\t"
            "xor r11, r11\n\t"
            "xor r12, r12\n\t"
            "xor r13, r13\n\t"
            "xor r14, r14\n\t"
            "xor r15, r15\n\t"

            "iretq\n\t"
            ".code32\n\t"
            :
            : "a" (pagemap_top_lv), "S" (casted_to_64)
            : "memory"
        );
    } else if (bits == 32) {
        asm volatile (
            "cli\n\t"
            "cld\n\t"

            "mov esp, esi\n\t"
            "push edi\n\t"
            "push 0\n\t"

            "pushfd\n\t"
            "push 0x18\n\t"
            "push ebx\n\t"

            "xor eax, eax\n\t"
            "xor ebx, ebx\n\t"
            "xor ecx, ecx\n\t"
            "xor edx, edx\n\t"
            "xor esi, esi\n\t"
            "xor edi, edi\n\t"
            "xor ebp, ebp\n\t"

            "iret\n\t"
            :
            : "D" ((uint32_t)casted_to_64[0]),
              "b" ((uint32_t)casted_to_64[1]),
              "S" ((uint32_t)casted_to_64[2])
            : "memory"
        );
    }

    __builtin_unreachable();
}
