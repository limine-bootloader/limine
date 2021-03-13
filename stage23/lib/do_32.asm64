section .rodata

invalid_idt:
    dq 0, 0

section .text

%macro push32 1
    sub rsp, 4
    mov dword [rsp], %1
%endmacro

global do_32
bits 64
do_32:
    mov rbp, rsp

    lidt [rel invalid_idt]

    cmp esi, 4
    jle .no_stack_args

.push_stack_args:
    dec esi
    mov eax, [rbp + 8 + rsi*8]
    push32 eax
    test esi, esi
    jnz .push_stack_args

.no_stack_args:
    push32 r9d
    push32 r8d
    push32 ecx
    push32 edx

    mov eax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lea rbx, [rel .go_32]

    push 0x18
    push rbx
    retfq

bits 32
.go_32:
    mov eax, cr0
    btr eax, 31
    mov cr0, eax

    mov ecx, 0xc0000080
    rdmsr
    btr eax, 8
    wrmsr

    mov eax, cr4
    btr eax, 5
    mov cr4, eax

    call edi
