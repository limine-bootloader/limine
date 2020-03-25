; This is a compliant "kernel" meant for testing purposes.

; Header


section .text

; Entry point
bits 32

global _start
_start:
    mov eax, 0xdeadbeef
    jmp $
