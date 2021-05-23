org 0x7c00
bits 16

start:
    jmp .skip_bpb ; Workaround for some BIOSes that require this stub
    nop

    ; Some BIOSes will do a funny and decide to overwrite bytes of code in
    ; the section where a FAT BPB would be, potentially overwriting
    ; bootsector code.
    ; Avoid that by filling the BPB area with 0s
    times 87 db 0

  .skip_bpb:
    cli
    cld
    jmp 0x0000:.initialise_cs
  .initialise_cs:
    xor bx, bx
    mov ds, bx
    mov es, bx
    mov ss, bx
    mov sp, 0x7c00
    sti

    ; Limine isn't made for floppy disks, these are dead anyways.
    ; So if the value the BIOS passed is <0x80, just assume it has passed
    ; an incorrect value.
    cmp dl, 0x80
    jb floppy_err
    ; Values above 0x8f are dubious so we assume we weren't booted properly
    ; for those either
    cmp dl, 0x8f
    ja hdd_err

  .continue:
    ; Make sure int 13h extensions are supported
    mov ah, 0x41
    mov bx, 0x55aa
    int 0x13
    jc err
    cmp bx, 0xaa55
    jne err

    ; If int 13h extensions are supported, then we are definitely running on
    ; a 386+. We have no idea whether the upper 16 bits of esp are cleared, so
    ; make sure that is the case now.
    mov esp, 0x7c00

    push 0x7000
    pop es
    mov di, stage2_locs
    mov eax, dword [di]
    mov ebp, dword [di+4]
    xor bx, bx
    xor ecx, ecx
    mov cx, word [di-4]
    call read_sectors
    jc err
    mov eax, dword [di+8]
    mov ebp, dword [di+12]
    add bx, cx
    mov cx, word [di-2]
    call read_sectors
    jc err

    lgdt [gdt]

    cli
    mov eax, cr0
    bts ax, 0
    mov cr0, eax

    jmp 0x08:vector

times 0xda-($-$$) db 0
times 6 db 0

; Includes

%include 'disk.asm'
%include '../gdt.asm'

err:
    push 0xb800
    pop es
    mov dword [es:0], eax
    .h: hlt
    jmp .h

floppy_err:
    mov eax, 'F ! '
    jmp err

hdd_err:
    mov eax, 'H ! '
    jmp err

bits 32
vector:
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    and edx, 0xff

    push 0

    push edx

    push stage2.size
    push (stage2 - decompressor) + 0x70000

    call 0x70000

times 0x1a4-($-$$) db 0
stage2_size_a: dw 0
stage2_size_b: dw 0
stage2_locs:
stage2_loc_a:  dq 0
stage2_loc_b:  dq 0

times 0x1b8-($-$$) db 0
times 510-($-$$) db 0
dw 0xaa55

; ********************* Stage 2 *********************

decompressor:
incbin '../../build/decompressor/decompressor.bin'

align 16
stage2:
incbin '../../build/stage23-bios/stage2.bin.gz'
.size: equ $ - stage2
