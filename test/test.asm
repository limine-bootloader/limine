; This is a compliant "kernel" meant for testing purposes.

; Header


section .text

; Entry point

global _start
_start:
    mov rax, 0xcafebabedeadbeef
    jmp $
