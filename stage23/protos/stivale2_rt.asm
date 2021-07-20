section .bss

user_stack:
    resq 1

user_cs: resq 1
user_ds: resq 1
user_es: resq 1
user_ss: resq 1

%define MAX_TERM_BUF 8192

section .text

extern term_write
extern stivale2_term_buf
extern stivale2_rt_stack

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
    mov rsp, [stivale2_rt_stack]

    mov word [user_cs], cs
    mov word [user_ds], ds
    mov word [user_es], es
    mov word [user_ss], ss

    push rsi
    mov rcx, rsi
    mov rax, MAX_TERM_BUF
    cmp rcx, rax
    cmovg rcx, rax
    mov rsi, rdi
    mov edi, [stivale2_term_buf]
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
    mov ss, ax
    push esi
    push dword [stivale2_term_buf]
    call term_write
    add esp, 8
    push dword [user_cs]
    push .mode64
    retfd
bits 64
  .mode64:
    mov ds, word [user_ds]
    mov es, word [user_es]
    mov ss, word [user_ss]
    mov rsp, [user_stack]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
