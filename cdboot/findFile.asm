BITS 16

; --- Find file in directory ---
; IN:
; eax <- directory extent
; ebx <- ptr to filename
; cl  <- filename sz
; esi <- size of directory extent

; OUT:
; eax <- LBA of the extent
; ebx <- size in bytes
; Carry if not found

; SMASHES:
; ch
findFile:
	push edx
	push edi
	push esi
	mov edx, eax

	.checkEntry:
	; Get the size of this entry
	mov edi, dword [edx + DIRECTORY_RECORD_LENGTH]
	and edi, 0xFF

	; Check filename size
	mov al, byte [edx + DIRECTORY_RECORD_FILENAME_LENGTH]
	cmp al, cl
	jnz .gonext

	; Sizes match, check filename
	lea eax, dword [edx + DIRECTORY_RECORD_FILENAME]
	call strcmp
	jnc .found

	; Go to the next
	.gonext:
	add edx, edi
	sub esi, edi
	test esi, esi
	jnz .checkEntry

	.notfound:
		stc
		jmp .end
	.found:
		clc
		mov eax, [edx + DIRECTORY_RECORD_LBA]
		mov ebx, [edx + DIRECTORY_RECORD_SIZE]
	.end:
		pop esi
		pop edi
		pop edx
		ret

%include 'strcmp.asm'
