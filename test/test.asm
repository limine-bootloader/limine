; This is a compliant "kernel" meant for testing purposes.

; Header


section .text

; Entry point

global _start
_start:
    mov rax, 'h e l l '
    mov rbx, 'o   w o '
    mov rcx, 'r l d   '
    mov [0xb8000], rax
    mov [0xb8008], rbx
    mov [0xb8010], rcx
    jmp $
