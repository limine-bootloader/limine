; ***********************************************
;     Reads a disk sector with an LBA address
; ***********************************************

; IN:
; EAX = LBA sector to load
; DL = Drive number
; ES = Buffer segment
; BX = Buffer offset

; OUT:
; Carry if error

read_sector:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    push es
    pop word [.target_segment]
    mov word [.target_offset], bx
    mov dword [.lba_address_low], eax

    xor esi, esi
    mov si, .da_struct
    mov ah, 0x42

    clc
    int 0x13

  .done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

align 4
.da_struct:
    .packet_size        db  16
    .unused             db  0
    .count              dw  1
    .target_offset      dw  0
    .target_segment     dw  0
    .lba_address_low    dd  0
    .lba_address_high   dd  0

; ********************************************
;     Reads multiple LBA addressed sectors
; ********************************************

; IN:
; EAX = LBA starting sector
; DL = Drive number
; ES = Buffer segment
; EBX = Buffer offset
; ECX = Sectors count

; OUT:
; Carry if error

%define TEMP_BUFFER_SEG 0x7000
%define BYTES_PER_SECT  512

read_sectors:
    push eax									; Save GPRs
    push ebx
    push ecx
    push edx
    push esi
    push edi

  .loop:
    push es
    push ebx

    mov bx, TEMP_BUFFER_SEG
    mov es, bx
    xor bx, bx

    call read_sector

    pop ebx
    pop es

    jc .done

    push ds

    mov si, TEMP_BUFFER_SEG
    mov ds, si
    mov edi, ebx
    xor esi, esi

    push ecx
    mov ecx, BYTES_PER_SECT
    a32 o32 rep movsb
    pop ecx

    pop ds

    inc eax
    add ebx, BYTES_PER_SECT

    loop .loop

  .done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret
