section .realmode

global pxe_call
global set_pxe_fp

set_pxe_fp:
    mov eax, [esp + 4]
    mov [pxe_call.pxe_fp], eax
    ret

pxe_call:
   ; Save GDT in case BIOS overwrites it
    sgdt [.gdt]

    ; Save non-scratch GPRs
    push ebx
    push esi
    push edi
    push ebp

    mov ebx, eax
    
    ; Jump to real mode
    jmp 0x08:.bits16
  .bits16:
    bits 16
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, cr0
    and al, 0xfe
    mov cr0, eax
    jmp 0x00:.cszero
  .cszero:
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    sti

    push dx
    push cx
    push bx
    call far [.pxe_fp]
    add sp, 6
    mov bx, ax

    cli
   ; Restore GDT
    lgdt [ss:.gdt]

    ; Jump back to pmode
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x18:.bits32
  .bits32:
    bits 32
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, ebx
    ; Restore non-scratch GPRs
    pop ebp
    pop edi
    pop esi
    pop ebx

    ; Exit
    ret

align 16
  .pxe_fp:   dd 0
  .esp:      dd 0
  .gdt:      dq 0
