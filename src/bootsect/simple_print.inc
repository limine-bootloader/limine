; **************************************
;     Prints a string using the BIOS
; **************************************

; IN:
; SI = points to a 0x00 terminated string

simple_print:
    push ax
    push si
    ; int 0x10, function 0x0e (print character)
    mov ah, 0x0e
  .loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
  .done:
    pop si
    pop ax
    ret
