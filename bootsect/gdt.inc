%define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))
%define ALIGN_UP(x, a)    (DIV_ROUNDUP((x), (a)) * (a))

; The GDT is copied to <start of EBDA> - gdt.size, which will also serve
; as the upper limit for balloc()
load_gdt:
    pusha
    push es
    mov ax, word [0x40e] ; 0x40e contains the value of a segment pointing to the EBDA
    sub ax, ALIGN_UP(gdt.size, 16) / 16
    mov es, ax
    mov word  [gdt.ptr], ax
    shl dword [gdt.ptr], 4
    xor di, di
    mov si, gdt.start
    mov cx, gdt.size
    rep movsb
    lgdt [gdt]
    pop es
    popa
    ret

gdt:
    dw .size - 1    ; GDT size
  .ptr:
    dd 0            ; GDT start address (calculated at runtime)

  .start:
    ; Null descriptor (required)
    dw 0x0000       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 00000000b    ; Access
    db 00000000b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 16-bit code
    dw 0xffff       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10011010b    ; Access
    db 00000000b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 16-bit data
    dw 0xffff       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10010010b    ; Access
    db 00000000b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 32-bit code
    dw 0xffff       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10011010b    ; Access
    db 11001111b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 32-bit data
    dw 0xffff       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10010010b    ; Access
    db 11001111b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 64-bit code
    dw 0x0000       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10011010b    ; Access
    db 00100000b    ; Granularity
    db 0x00         ; Base (high 8 bits)

    ; 64-bit data
    dw 0x0000       ; Limit
    dw 0x0000       ; Base (low 16 bits)
    db 0x00         ; Base (mid 8 bits)
    db 10010010b    ; Access
    db 00000000b    ; Granularity
    db 0x00         ; Base (high 8 bits)

  .end:

  .size: equ .end - .start
