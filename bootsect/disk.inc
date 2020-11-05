; ***********************************************
;     Reads a disk sector with an LBA address
; ***********************************************

; IN:
; EAX = LBA sector to load
; DL = Drive number
; ES = Buffer segment
; BX = Buffer offset
; ECX = Byte count

; OUT:
; Carry if error

read_sectors:
    pusha

    mov esi, .da_struct

    mov word  [si],    16
    mov word  [si+2],  1
    mov word  [si+4],  bx
    mov word  [si+6],  es
    mov dword [si+8],  eax
    mov dword [si+12], 0

    ; Get bytes per sector
    push dx
    push si
    mov ah, 0x48
    mov si, .drive_params
    mov word [si], 30       ; buf_size
    int 0x13
    jc .done
    mov bp, word [si+24]    ; bytes_per_sect
    mov ax, cx
    shr ecx, 16
    mov dx, cx
    xor cx, cx
    div bp
    test dx, dx
    adc cx, ax
    pop si
    pop dx

  .loop:
    mov ah, 0x42

    clc
    int 0x13
    jc .done

    add word  [si+4], bp
    inc dword [si+8]

    loop .loop

  .done:
    popa
    ret

  .da_struct:    equ 0x8000
  .drive_params: equ 0x8010
