; ***********************************************
;     Reads a disk sector with an LBA address
; ***********************************************

; IN:
; EAX = LBA sector to load
; DL = Drive number
; ES = Buffer segment
; BX = Buffer offset
; CX = Sectors count

; OUT:
; Carry if error

read_sectors:
    pusha

    push es
    pop word [.target_segment]
    mov word [.target_offset], bx
    mov dword [.lba_address_low], eax

    mov word [.countdown], cx

  .loop:
    mov esi, .da_struct
    mov ah, 0x42

    clc
    int 0x13
    jc .done

    add word [.target_offset], 512
    inc dword [.lba_address_low]

    dec word [.countdown]
    jnz .loop

  .done:
    popa
    ret

align 2
  .countdown: dw 0

align 4
  .da_struct:
    .packet_size        db  16
    .unused             db  0
    .count              dw  1
    .target_offset      dw  0
    .target_segment     dw  0
    .lba_address_low    dd  0
    .lba_address_high   dd  0
