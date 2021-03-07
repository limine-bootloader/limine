BITS 16
ORG 0x7C00

%define STAGE2_LOCATION       0x60000
%define DECOMPRESSOR_LOCATION 0x70000
%define BOOT_FROM_CD 2

jmp skip_bpb
nop

; El Torito Boot Information Table
; ↓ Set by mkisofs
times 8-($-$$) db 0
boot_info:
    bi_PVD      dd 0
    bi_boot_LBA dd 0
    bi_boot_len dd 0
    bi_checksum dd 0
    bi_reserved times 40 db 0

times 90-($-$$) db 0

skip_bpb:
    cli
    cld
    jmp 0x0000:.initialise_cs
  .initialise_cs:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; int 13h?
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc err
    cmp bx, 0xAA55
    jne err

    mov esp, 0x7C00

    ; --- Load the decompressor ---
    mov eax, dword [bi_boot_LBA]
    add eax, DEC_LBA_OFFSET
    mov ecx, DEC_LBA_COUNT
    ; DECOMPRESSOR_LOCATION = 0x70000 = 0x7000:0x0000
    mov si, 0x7000
    xor di, di
    call read_2k_sectors
    jc err

    ; --- Load the stage2.bin.gz ---
    mov eax, dword [bi_boot_LBA]
    add eax, STAGE2_LBA_OFFSET
    mov ecx, STAGE2_LBA_COUNT
    ; STAGE2_LOCATION = 0x60000 = 0x6000:0x0000
    mov si, 0x6000
    xor di, di
    call read_2k_sectors
    jc err

    ; Enable GDT
    lgdt [gdt]
    cli
    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:pmode

err:
    hlt
    jmp err

%include 'read_2k_sectors.asm'
%include '../gdt.asm'

BITS 32
pmode:
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Time to handle control over to the decompressor
    push BOOT_FROM_CD
    and edx, 0xFF
    push edx  ; Boot drive
    push STAGE2_SIZE
    push STAGE2_LOCATION
    call DECOMPRESSOR_LOCATION
    hlt

%define FILEPOS ($-$$)
%define UPPER2K ((FILEPOS+2047) & ~2047)
%define ALIGN2K times UPPER2K - FILEPOS db 0

; Align stage2 to 2K ON DISK
ALIGN2K
DEC_LBA_OFFSET equ ($-$$)/2048
incbin '../../build/decompressor/decompressor.bin'

ALIGN2K
STAGE2_START equ $-$$
STAGE2_LBA_OFFSET equ STAGE2_START/2048
DEC_LBA_COUNT equ STAGE2_LBA_OFFSET - DEC_LBA_OFFSET
incbin '../../build/stage23-bios/stage2.bin.gz'
STAGE2_SIZE equ ($-$$) - STAGE2_START
STAGE2_LBA_COUNT equ (2047 + $-$$)/2048
