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

    test dword [smp_tpl_target_mode], (1 << 0)
    jz parking32

    mov eax, cr4
    bts eax, 5
    mov cr4, eax

    test dword [smp_tpl_target_mode], (1 << 1)
    jz .no5lv

    mov eax, cr4
    bts eax, 12
    mov cr4, eax

  .no5lv:
    mov eax, dword [smp_tpl_pagemap]
    mov cr3, eax

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
    mov edi, dword [smp_tpl_info_struct]
    mov eax, 1
    lock xchg dword [smp_tpl_booted_flag], eax

    xor eax, eax
  .loop:
    lock xadd dword [edi + 16], eax
    test eax, eax
    jnz .out
    pause
    jmp .loop

  .out:
    mov esp, dword [edi + 8]
    push 0
    push eax
    push edi
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    ret

bits 64
parking64:
    mov edi, dword [smp_tpl_info_struct]
    mov eax, 1
    lock xchg dword [smp_tpl_booted_flag], eax

    xor eax, eax
  .loop:
    lock xadd qword [rdi + 16], rax
    test rax, rax
    jnz .out
    pause
    jmp .loop

  .out:
    mov rsp, qword [rdi + 8]
    push 0
    push rax
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    ret
