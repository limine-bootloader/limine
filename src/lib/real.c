#include <stdint.h>
#include <lib/real.h>
#include <lib/asm.h>

__attribute__((naked))
void rm_int(uint8_t int_no, struct rm_regs *out_regs, struct rm_regs *in_regs) {
    ASM_BASIC(
        // Self-modifying code: int $int_no
        "mov al, byte ptr ss:[esp+4]\n\t"
        "mov byte ptr ds:[3f], al\n\t"

        // Save out_regs
        "mov eax, dword ptr ss:[esp+8]\n\t"
        "mov dword ptr ds:[6f], eax\n\t"

        // Save in_regs
        "mov eax, dword ptr ss:[esp+12]\n\t"
        "mov dword ptr ds:[7f], eax\n\t"

        // Save GDT in case BIOS overwrites it
        "sgdt [8f]\n\t"

        // Save non-scratch GPRs
        "push ebx\n\t"
        "push esi\n\t"
        "push edi\n\t"
        "push ebp\n\t"

        // Jump to real mode
        FARJMP32("0x08", "1f")
        "1: .code16\n\t"
        "mov ax, 0x10\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "mov eax, cr0\n\t"
        "and al, 0xfe\n\t"
        "mov cr0, eax\n\t"
        FARJMP16("0", "1f")
        "1:\n\t"
        "mov ax, 0\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"

        // Load in_regs
        "mov dword ptr ds:[5f], esp\n\t"
        "mov esp, dword ptr ds:[7f]\n\t"
        "popfd\n\t"
        "pop ebp\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "pop edx\n\t"
        "pop ecx\n\t"
        "pop ebx\n\t"
        "pop eax\n\t"
        "mov esp, dword ptr ds:[5f]\n\t"

        "sti\n\t"

        // Indirect interrupt call
        ".byte 0xcd\n\t"
        "3: .byte 0\n\t"

        "cli\n\t"

        // Load out_regs
        "mov dword ptr ds:[5f], esp\n\t"
        "mov esp, dword ptr ds:[6f]\n\t"
        "lea esp, [esp + 8*4]\n\t"
        "push eax\n\t"
        "push ebx\n\t"
        "push ecx\n\t"
        "push edx\n\t"
        "push esi\n\t"
        "push edi\n\t"
        "push ebp\n\t"
        "pushfd\n\t"
        "mov esp, dword ptr ds:[5f]\n\t"

        // Restore GDT
        "lgdt [8f]\n\t"

        // Jump back to pmode
        "mov eax, cr0\n\t"
        "or al, 1\n\t"
        "mov cr0, eax\n\t"
        FARJMP16("0x18", "1f")
        "1: .code32\n\t"
        "mov ax, 0x20\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"

        // Restore non-scratch GPRs
        "pop ebp\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "pop ebx\n\t"

        // Exit
        "ret\n\t"

        // ESP
        "5: .long 0\n\t"
        // out_regs
        "6: .long 0\n\t"
        // in_regs
        "7: .long 0\n\t"
        // gdt
        "8: .long 0\n\t"
        "   .long 0\n\t"
    );
}
