section .realmode

global rm_int
rm_int:
    ; Self-modifying code: int $int_no
    mov al, byte [esp+4]
    mov byte [.int_no], al

    ; Save out_regs
    mov eax, dword [esp+8]
    mov dword [.out_regs], eax

    ; Save in_regs
    mov eax, dword [esp+12]
    mov dword [.in_regs], eax

    ; Save GDT in case BIOS overwrites it
    sgdt [.gdt]

    ; Save non-scratch GPRs
    push ebx
    push esi
    push edi
    push ebp

    ; Jump to real mode
    jmp 0x08:.bits16
  .bits16:
    bits 16
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, cr0
    and al, 0xfe
    mov cr0, eax
    jmp 0x00:.cszero
  .cszero:
    xor ax, ax
    mov ss, ax

    ; Load in_regs
    mov dword [ss:.esp], esp
    mov esp, dword [ss:.in_regs]
    pop gs
    pop fs
    pop es
    pop ds
    popfd
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    mov esp, dword [ss:.esp]

    sti

    ; Indirect interrupt call
    db 0xcd
  .int_no:
    db 0

    cli

    ; Load out_regs
    mov dword [ss:.esp], esp
    mov esp, dword [ss:.out_regs]
    lea esp, [esp + 10*4]
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    pushfd
    push ds
    push es
    push fs
    push gs
    mov esp, dword [ss:.esp]

    ; Restore GDT
    lgdt [ss:.gdt]

    ; Jump back to pmode
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x18:.bits32
  .bits32:
    bits 32
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Restore non-scratch GPRs
    pop ebp
    pop edi
    pop esi
    pop ebx

    ; Exit
    ret

align 16
  .esp:      dd 0
  .out_regs: dd 0
  .in_regs:  dd 0
  .gdt:      dq 0
