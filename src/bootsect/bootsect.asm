org 0x7c00
bits 16

start:
    cli
    cld
    jmp 0x0000:.initialise_cs
  .initialise_cs:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov si, LoadingMsg
    call simple_print

    ; ****************** Load stage 2 ******************

    mov si, Stage2Msg
    call simple_print

    mov eax, dword [stage2_sector]
    mov ebx, 0x7e00
    mov ecx, 1
    call read_sectors

    jc err_reading_disk

    mov si, DoneMsg
    call simple_print

    jmp 0x7e00

err_reading_disk:
    mov si, ErrReadDiskMsg
    call simple_print
    jmp halt

err_enabling_a20:
    mov si, ErrEnableA20Msg
    call simple_print
    jmp halt

halt:
    hlt
    jmp halt

; Data

LoadingMsg db 0x0D, 0x0A, '<qloader2>', 0x0D, 0x0A, 0x0A, 0x00
Stage2Msg db 'stage1: Loading stage2...', 0x00
ErrReadDiskMsg db 0x0D, 0x0A, 'Error reading disk, system halted.', 0x00
ErrEnableA20Msg db 0x0D, 0x0A, 'Error enabling a20, system halted.', 0x00
DoneMsg db '  DONE', 0x0D, 0x0A, 0x00

times 0xda-($-$$) db 0
times 6 db 0

; Includes

%include 'simple_print.inc'
%include 'disk.inc'

times 0x1b0-($-$$) db 0
stage2_sector: dd 1

times 0x1b8-($-$$) db 0
times 510-($-$$) db 0
dw 0xaa55

; ********************* Stage 2 *********************

stage2:
    mov eax, dword [stage2_sector]
    inc eax
    mov ebx, 0x8000
    mov ecx, 62
    call read_sectors
    jc err_reading_disk

    call enable_a20
    jc err_enabling_a20

    lgdt [GDT]

    cli

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x18:.pmode
    bits 32
  .pmode:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    and edx, 0xff
    push edx

    call 0x8000

bits 16
%include 'a20_enabler.inc'
%include 'gdt.inc'

times 1024-($-$$) db 0

incbin '../stage2.bin'

times 32768-($-$$) db 0
