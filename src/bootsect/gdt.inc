load_gdt:
    pusha
    push es
    push ds
    push 0x7000
    pop es
    xor di, di
    push 0
    pop ds
    mov si, GDT.GDTStart
    mov cx, GDT.GDTEnd - GDT.GDTStart
    rep movsb
    lgdt [GDT]
    pop ds
    pop es
    popa
    ret

GDT:

dw .GDTEnd - .GDTStart - 1	; GDT size
dd 0x70000				; GDT start

.GDTStart:

; Null descriptor (required)

.NullDescriptor:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 00000000b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; 16-bit code
dw 0xffff			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; 16-bit data
dw 0xffff			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10010010b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; 32-bit code
dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 11001111b		; Granularity
db 0x00				; Base (high 8 bits)

; 32-bit data
dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10010010b		; Access
db 11001111b		; Granularity
db 0x00				; Base (high 8 bits)

; 64 bit code
dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10011010b        ; Access
db 00100000b        ; Granularity
db 0x00             ; Base (high 8 bits)

; 64 bit data
dw 0x0000           ; Limit
dw 0x0000           ; Base (low 16 bits)
db 0x00             ; Base (mid 8 bits)
db 10010010b        ; Access
db 00000000b        ; Granularity
db 0x00             ; Base (high 8 bits)

.GDTEnd:
