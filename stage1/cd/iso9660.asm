BITS 16

%include 'read_2k_sectors.asm'
%include 'findPVD.asm'
%include 'findFile.asm'

; --- Read file ---
; IN:
; ebx <- ptr to filename
; cl <- filename sz
; esi <- size of directory extent

; OUT:
; If found: [ISO9660_BUFFER] <- contents of the file
; If not found: CF=1
; esi <- size in bytes

; SMASHES:
; eax, ebx, ecx
read_file:
    mov eax, ISO9660_BUFFER
    call findFile
    jc .end

    ; LBA of the extent is now @ eax
    ; ebx is size in bytes
    mov esi, ebx  ; Return value

    ; bytes to 2k sectors
    mov ecx, ebx
    add ecx, 2047
    shr ecx, 11
    call read_2k_sectors
  .end:
    ret
