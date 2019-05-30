a20_check:

; *************************************************
;     Checks if the A20 address line is enabled
; *************************************************

; OUT:
; Carry if disabled, cleared if enabled

push ax										; Save registers
push bx
push es
push fs

xor ax, ax									; Set ES segment to zero
mov es, ax
not ax										; Set FS segment to 0xFFFF
mov fs, ax

mov ax, word [es:0x7DFE]					; Check using boot signature
cmp word [fs:0x7E0E], ax					; If A20 is disabled, this should be the
											; same address as the boot signature
je .change_values							; If they are equal, check again with another value

.enabled:

clc											; A20 is enabled, clear carry flag
jmp .done

.change_values:

mov word [es:0x7DFE], 0x1234				; Change the value of 0000:7DFE to 0x1234
cmp word [fs:0x7E0E], 0x1234				; Is FFFF:7E0E changed as well?
jne .enabled								; If it is, A20 is enabled

stc											; Otherwise set carry

.done:

mov word [es:0x7DFE], ax					; Restore boot signature
pop fs										; Restore registers
pop es
pop bx
pop ax
ret											; Exit routine




enable_a20:

; ********************************************
;     Tries to enable the A20 address line
; ********************************************

; OUT:
; Carry cleared if success, set if fail

push eax								; Save registers

call a20_check							; Check if a20 is already enabled
jnc .done								; If it is, we are done

mov ax, 0x2401							; Use BIOS to try to enable a20
int 0x15

call a20_check							; Check again to see if BIOS succeeded
jnc .done								; If it has, we are done

.keyboard_method:

cli										; Disable interrupts

call .a20wait							; Use the keyboard controller to try and
mov al, 0xAD							; open the A20 gate
out 0x64, al

call .a20wait
mov al, 0xD0
out 0x64, al

call .a20wait2
in al, 0x60
push eax

call .a20wait
mov al, 0xD1
out 0x64, al

call .a20wait
pop eax
or al, 2
out 0x60, al

call .a20wait
mov al, 0xAE
out 0x64, al

call .a20wait
sti										; Enable interrupts back

jmp .keyboard_done

.a20wait:

in al, 0x64
test al, 2
jnz .a20wait
ret

.a20wait2:

in al, 0x64
test al, 1
jz .a20wait2
ret

.keyboard_done:

call a20_check							; Check for success

; Now just quit the routine, forwarding the carry flag to the caller

.done:
pop eax
ret
