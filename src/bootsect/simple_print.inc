; **************************************
;     Prints a string using the BIOS
; **************************************

; IN:
; SI = points to a 0x00 terminated string

simple_print:
    push ax						; Save registers
    push si
    mov ah, 0x0E				; int 0x10, function 0x0E (print character)
  .loop:
	lodsb					; Load character from string
	test al, al				; Is is the 0x00 terminator?
	jz .done				; If it is, exit routine
	int 0x10				; Call BIOS
	jmp .loop				; Repeat!
  .done:
	pop si					; Restore registers
	pop ax
	ret						; Exit routine
