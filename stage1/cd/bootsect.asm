BITS 16
ORG 0x7C00

; Please read bootsect/bootsect.asm before this file

%define ISO9660_BUFFER 0x8000
%define ROOT_DIRECTORY 156
%define ROOT_DIRECTORY_BUFFER (ISO9660_BUFFER + ROOT_DIRECTORY)

%define DIRECTORY_RECORD_LENGTH 0
%define DIRECTORY_RECORD_LBA 2
%define DIRECTORY_RECORD_SIZE 10
%define DIRECTORY_RECORD_FILENAME_LENGTH 32
%define DIRECTORY_RECORD_FILENAME 33

%define BOOT_FROM_CD 2

jmp skip_bpb
nop
times 87 db 0

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

    ; --- Load the stage 2 ---
    ; Find and load the PVD
    call findPVD
    jc err

    ; Load the root directory
    mov eax, dword [ROOT_DIRECTORY_BUFFER + DIRECTORY_RECORD_LBA]
    mov ecx, dword [ROOT_DIRECTORY_BUFFER + DIRECTORY_RECORD_SIZE]

    mov esi, ecx  ; Size, for read_file
    add ecx, 2047
    shr ecx, 11
    call read_2k_sectors
    jc err

    ; Find and load '/BOOT'
    mov ebx, TXT_BOOT
    mov cl, TXT_BOOT_SZ
    call read_file
    jc err

    ; Find and load '/BOOT/LIMINE.SYS'
    mov ebx, TXT_LIMINE
    mov cl, TXT_LIMINE_SZ
    call read_file  ; esi is set from the last call
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

%include 'iso9660.asm'
%include '../gdt.asm'

BITS 32
pmode:
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Time to handle control over to the stage 2
    push BOOT_FROM_CD
    and edx, 0xFF
    push edx  ; Boot drive
    call ISO9660_BUFFER
    hlt

TXT_BOOT: db "BOOT"
TXT_BOOT_SZ equ $ - TXT_BOOT
TXT_LIMINE: db "LIMINE.SYS;1"
TXT_LIMINE_SZ equ $ - TXT_LIMINE

; Just making sure the entry point (ISO9660_BUFFER) is not reached
times (0x8000 - 0x7C00) - ($ - $$) db 0
