extern smp_tpl_info_struct
extern smp_tpl_booted_flag
extern smp_tpl_pagemap
extern smp_tpl_target_mode

section .realmode

global smp_trampoline
align 0x1000
bits 16
smp_trampoline:
    cli
    cld

    xor ax, ax
    mov ds, ax

    lgdt [smp_tpl_gdt]

    mov eax, cr0
    bts eax, 0
    mov cr0, eax

    jmp 0x18:.mode32
    bits 32
  .mode32:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, cr0
    btr eax, 29
    btr eax, 30
    mov cr0, eax

    cmp dword [smp_tpl_target_mode], 0
    je parking32

    mov eax, dword [smp_tpl_pagemap]
    mov cr3, eax

    mov eax, cr4
    bts eax, 5
    mov cr4, eax

    mov ecx, 0xc0000080
    rdmsr
    bts eax, 8
    wrmsr

    mov eax, cr0
    bts eax, 31
    mov cr0, eax

    jmp 0x28:.mode64
    bits 64
  .mode64:
    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp parking64

global smp_tpl_gdt
align 16
smp_tpl_gdt:
    dw 0
    dd 0

section .text

bits 32
parking32:
    mov ecx, dword [smp_tpl_info_struct]
    mov eax, 1
    lock xchg dword [smp_tpl_booted_flag], eax
    mov eax, 0xcafebabe
    jmp $

bits 64
parking64:
    mov ecx, dword [smp_tpl_info_struct]
    mov eax, 1
    lock xchg dword [smp_tpl_booted_flag], eax
    mov eax, 0xdeadbeef
    jmp $
