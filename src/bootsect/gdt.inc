GDT:

dw .GDTEnd - .GDTStart - 1	; GDT size
dd .GDTStart				; GDT start

.GDTStart:

; Null descriptor (required)

.NullDescriptor:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 00000000b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; Unreal mode

.UnrealCode:

dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 10001111b		; Granularity
db 0x00				; Base (high 8 bits)

.UnrealData:

dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10010010b		; Access
db 10001111b		; Granularity
db 0x00				; Base (high 8 bits)

; Protected mode

.PmodeCode:

dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 11001111b		; Granularity
db 0x00				; Base (high 8 bits)

.PmodeData:

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
