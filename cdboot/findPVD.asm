BITS 16

; --- Find primary volume descriptor ---
; IN:
; dl <- drive number

; OUT:
; [ISO9660_BUFFER] <- PVD
; Carry if not found
findPVD:
    pusha
    ; Just start reading volume descriptors

    mov eax, 0x10    ; First volume descriptor's LBA
  .checkVD:
    mov cx, 1    ; Each volume descriptor is 2KB
    call read_2k_sectors

    ; Check identifier
    mov ecx, ISO9660_BUFFER
    inc ecx
    mov bl, byte [ecx]
    inc ecx
    mov ecx, dword [ecx]
    cmp bl, 'C'
    jne .notfound
    cmp ecx, 0x31303044
    jne .notfound

    ; Check type = 0xFF (final)
    mov ecx, ISO9660_BUFFER
    mov bl, byte [ecx]
    cmp bl, 0xFF
    jz .notfound

    ; Check type = 0x01 (PVD)
    cmp bl, 0x01
    jz .found

    ; Didn't match, go to the next
    inc eax
    jmp .checkVD

  .found:
    clc
    jmp .end
  .notfound:
    stc
  .end:
    popa
    ret
