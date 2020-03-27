; This is a compliant "kernel" meant for testing purposes.

; Header
section .stivalehdr

stivale_header:
    dq stack.top    ; rsp
    db 1            ; video mode
    dw 640          ; fb_width
    dw 480          ; fb_height

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
    mov rdx, [rdi+20]
    mov [rdx], rax
    mov [rdx+8], rbx
    mov [rdx+16], rcx
    jmp $
