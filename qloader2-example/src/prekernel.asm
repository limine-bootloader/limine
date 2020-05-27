; prekernel.asm. This file will be the first one executed
; when the kernel is loaded into memory.

; qloader2's native boot protocol is Stivale.
; All the options to it are on a special section
; called stivalehdr.

section .stivalehdr
stivale_header:
	.rsp            	dq      stack.top               	; Where the stack pointer will point.
	.flags          	dw      0000000000000000b       	; Some flags. The first bit indicates text mode or not,
									; and the second one if it's clear, 4 level paging (Otherwise 5 level paging)
															
	.framebuffer_width	dw	0000000000000000b		; Only for graphical mode.
	.framebuffer_height 	dw	0000000000000000b		; Only for graphical mode.
	.framebuffer_bpp	dw	0000000000000000b		; Only for graphical mode.
	
	
	
; BSS section. Here goes the stack.
section .bss
stack:
	resb 4096							; Reserve 4096 bytes for the stack.

.top:									; The top of the stack.


; Text section. Here we set the GDT (optional) and then pass control to our C code.
; TODO: GDT

section .text
global _start								; Entry point.
extern kmain								; Our C function.
_start:

	call kmain							; Jump into the kernel!

; If for some reason the kernel ends, go here.
hang:
	cli
	hlt
	jmp hang
