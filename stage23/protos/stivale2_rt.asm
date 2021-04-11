section .bss

user_stack:
    resq 1

term_buf:
    resb 1024

section .text

extern term_write

bits 64
global stivale2_term_write_entry
stivale2_term_write_entry:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov [user_stack], rsp
    mov rsp, 0x7c00

    push rsi
    mov rcx, rsi
    mov rsi, rdi
    mov edi, term_buf
    rep movsb
    pop rsi

    push 0x18
    push .mode32
    retfq
bits 32
  .mode32:
    mov eax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    push esi
    push term_buf
    call term_write
    add esp, 8
    push 0x28
    push .mode64
    retfd
bits 64
  .mode64:
    mov eax, 0x30
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, [user_stack]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
