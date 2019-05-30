org 0x7C00
bits 16

code_start:

cli
jmp 0x0000:initialise_cs
initialise_cs:
xor ax, ax
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
mov sp, 0x7c00
sti

mov byte [drive_number], dl

mov si, LoadingMsg
call simple_print

; ****************** Load stage 2 ******************

mov si, Stage2Msg
call simple_print

mov ax, 1
mov ebx, 0x7e00
mov cx, 7
call read_sectors

jc err

mov si, DoneMsg
call simple_print

jmp 0x7e00

err:
mov si, ErrMsg
call simple_print

halt:
hlt
jmp halt

;Data

LoadingMsg		db 0x0D, 0x0A, '<qLoader 2>', 0x0D, 0x0A, 0x0A, 0x00
Stage2Msg		db 'stage1: Loading stage2...', 0x00
ErrMsg			db 0x0D, 0x0A, 'Error, system halted.', 0x00
DoneMsg			db '  DONE', 0x0D, 0x0A, 0x00

times 0xda-($-$$) db 0
times 6 db 0

;Includes

%include 'simple_print.inc'
%include 'disk.inc'

drive_number db 0

times 0x1b8-($-$$) db 0
times 510-($-$$) db 0
dw 0xaa55
