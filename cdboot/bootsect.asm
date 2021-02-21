BITS 16
ORG 0x7C00

; Please read bootsect/bootsect.asm before this file
; stage2 has to be at boot:///BOOT/STAGE2.GZ

%define DECOMPRESSOR_EP 0x70000
%define ISO9660_BUFFER 0x9000
%define STAGE2_GZ_LOCATION 0x60000
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

	mov esi, ecx	; Size, for read_file
	add ecx, 2047
	shr ecx, 11
	call read_2k_sectors
	jc err

	; Find and load '/BOOT'
	mov ebx, TXT_BOOT
	mov cl, TXT_BOOT_SZ
	call read_file
	jc err

	; Find and load '/BOOT/STAGE2.GZ'
	mov ebx, TXT_STAGE2
	mov cl, TXT_STAGE2_SZ
	call read_file	; esi is set from the last call
	jc err
	mov dword [stage2_gz_sz], esi	; Let's save this for later

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
%include '../bootsect/gdt.inc'

BITS 32
pmode:
	mov eax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Move the decompressor to a safe location
	mov esi, decompressor
	mov edi, DECOMPRESSOR_EP
	mov ecx, decompressor_sz
	rep movsb

	; At this point, '/BOOT/STAGE2.GZ' is at ISO9660_BUFFER,
	; it has to be moved higher up
	mov esi, ISO9660_BUFFER
	mov edi, STAGE2_GZ_LOCATION
	mov ecx, dword [stage2_gz_sz]
	rep movsb

	; Time to handle control over to the decompressor
	push BOOT_FROM_CD
	and edx, 0xFF
	push edx	; Boot drive
	push dword [stage2_gz_sz]
	push STAGE2_GZ_LOCATION
	call DECOMPRESSOR_EP
	hlt

TXT_BOOT: db "BOOT"
TXT_BOOT_SZ equ $ - TXT_BOOT
TXT_STAGE2: db "STAGE2.GZ;1"
TXT_STAGE2_SZ equ $ - TXT_STAGE2

decompressor: incbin '../decompressor/decompressor.bin'
decompressor_sz equ $ - decompressor

stage2_gz_sz: dd 0
