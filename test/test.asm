; This is a compliant "kernel" meant for testing purposes.

; Header
section .stivalehdr

stivale_header:
    dq stack.top    ; rsp
    dw 0            ; video mode
    dw 0          ; fb_width
    dw 0          ; fb_height
    dw 0          ; fb_bpp

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