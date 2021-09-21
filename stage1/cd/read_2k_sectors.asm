bits 16

; --- Read sectors from disk ---
; IN:
; eax <- start LBA (2k sectors)
; cx <- number of 2k sectors
; dl <- drive number
; ds <- ZERO
; bx <- buffer offset
; es <- buffer segment

; OUT:
; Carry if error

align 8
dapack:
    dapack_size:    db 0x10
    dapack_null:    db 0x00
    dapack_nblocks: dw 0
    dapack_offset:  dw 0
    dapack_segment: dw 0
    dapack_LBA:     dq 0

read_2k_sectors:
    pusha
    mov dword [dapack_LBA], eax
    mov word  [dapack_nblocks], cx
    mov word  [dapack_offset], bx
    mov word  [dapack_segment], es

    mov ah, 0x42
    mov si, dapack
    int 0x13
    popa
    ret
