.section .entry

.globl _start
_start:
    cld

    // Zero out .bss
    xor %al, %al
    mov $bss_begin, %edi
    mov $bss_end, %ecx
    sub $bss_begin, %ecx
    rep stosb

    jmp entry
