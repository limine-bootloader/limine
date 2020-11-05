org 0x7c00
bits 16

start:
    jmp 0x0000:.initialise_cs
  .initialise_cs:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    call load_gdt

    cli

    mov eax, cr0
    bts ax, 0
    mov cr0, eax

    jmp 0x18:.mode32
    bits 32
  .mode32:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push 0x1
    and edx, 0xff
    push edx

    push stage2.size
    push (stage2 - decompressor) + 0x70000

    mov esi, decompressor
    mov edi, 0x70000
    mov ecx, stage2.fullsize
    rep movsb

    call 0x70000

bits 16

err:
    hlt
    jmp err

; Includes

%include '../bootsect/gdt.inc'

; ********************* Stage 2 *********************

decompressor:
incbin '../decompressor/decompressor.bin'

align 16
stage2:
incbin '../stage2/stage2.bin.gz'
.size: equ $ - stage2
.fullsize: equ $ - decompressor
