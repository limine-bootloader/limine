section .rodata

invalid_idt:
    dd 0, 0

section .text

global common_spinup
bits 32
common_spinup:
    cli

    lidt [invalid_idt]

    xor eax, eax
    lldt ax

    ; We don't need the return address
    add esp, 4

    ; Get function address
    pop edi

    ; We don't need the argument count
    add esp, 4

    mov eax, 0x00000011
    mov cr0, eax

    xor eax, eax
    mov cr4, eax

    call edi
