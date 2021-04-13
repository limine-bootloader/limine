section .bss

user_stack:
    resq 1

user_cs: resq 1
user_ds: resq 1
user_es: resq 1
user_fs: resq 1
user_gs: resq 1
user_ss: resq 1

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

    mov word [user_cs], cs
    mov word [user_ds], ds
    mov word [user_es], es
    mov word [user_fs], fs
    mov word [user_gs], gs
    mov word [user_ss], ss

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
    push dword [user_cs]
    push .mode64
    retfd
bits 64
  .mode64:
    mov ds, word [user_ds]
    mov es, word [user_es]
    mov fs, word [user_fs]
    mov gs, word [user_gs]
    mov ss, word [user_ss]
    mov rsp, [user_stack]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret
