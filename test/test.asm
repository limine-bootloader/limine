; This is a compliant "kernel" meant for testing purposes.

; Header
section .stivale2hdr

stivale_header:
    dq 0         ; entry point
    dq stack.top ; rsp
    dq 0         ; flags
    dq 0         ; tags

section .bss

stack:
    resb 4096
  .top:

section .text

; Entry point

global _start
_start:
    mov rax, 'h e l l '
    mov rbx, 'o   w o '
    mov rcx, 'r l d   '
    mov rdx, 0xb8000
    mov [rdx], rax
    mov [rdx+8], rbx
    mov [rdx+16], rcx
    jmp $
