section .text

global __udivdi3
__udivdi3:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    div dword [esp+12]
    xor edx, edx
    ret

global __divdi3
__divdi3:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    idiv dword [esp+12]
    xor edx, edx
    ret

global __umoddi3
__umoddi3:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    div dword [esp+12]
    mov eax, edx
    xor edx, edx
    ret

global __moddi3
__moddi3:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    idiv dword [esp+12]
    mov eax, edx
    xor edx, edx
    ret

global __udivmoddi4
__udivmoddi4:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    div dword [esp+12]
    mov ecx, dword [esp+20]
    mov dword [ecx], edx
    mov dword [ecx+4], 0
    xor edx, edx
    ret

global __divmoddi4
__divmoddi4:
    mov eax, dword [esp+4]
    mov edx, dword [esp+8]
    idiv dword [esp+12]
    mov ecx, dword [esp+20]
    mov dword [ecx], edx
    mov dword [ecx+4], 0
    xor edx, edx
    ret

global memcpy32to64
memcpy32to64:
bits 32
    push ebp
    mov ebp, esp

    push esi
    push edi

    push 0x28
    call .p1
  .p1:
    add dword [esp], .mode64 - .p1
    retfd

bits 64
  .mode64:
    mov rdi, [rbp + 8]
    mov rsi, [rbp + 16]
    mov rcx, [rbp + 24]
    rep movsb

    push 0x18
    call .p2
  .p2:
    add qword [rsp], .mode32 - .p2
    retfq

bits 32
  .mode32:
    pop edi
    pop esi
    pop ebp
    ret
