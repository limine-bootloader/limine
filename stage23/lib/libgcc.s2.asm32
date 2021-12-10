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
