BITS 16

; --- Compare strings ---
; IN:
; eax <- ptr to first string
; ebx <- ptr to second string
; cl <- size of BOTH strings (must be the same)

; OUT:
; CF=0 if equal, CF=1 otherwise
strcmp:
    pusha
  .nextchar:
    mov dh, byte [eax]
    mov dl, byte [ebx]
    cmp dh, dl
    jnz .notequal

    ; Characters match
    dec cl
    test cl, cl
    jz .equal
    inc eax
    inc ebx
    jmp .nextchar

  .equal:
    clc
    jmp .end
  .notequal:
    stc
  .end:
    popa
    ret
