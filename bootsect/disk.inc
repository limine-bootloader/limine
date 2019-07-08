read_sector:

; ***********************************************
;     Reads a disk sector with an LBA address
; ***********************************************

; IN:
; EAX = LBA sector to load
; DL = Drive number
; ES = Buffer segment
; BX = Buffer offset

; OUT:
; Carry if error

push es
pop word [.target_segment]
mov word [.target_offset], bx
mov dword [.lba_address_low], eax

xor esi, esi
mov si, .da_struct
mov ah, 0x42

clc										; Clear carry for int 0x13 because some BIOSes may not clear it on success

int 0x13								; Call int 0x13

.done:
ret										; Exit routine

align 4
.da_struct:
    .packet_size        db  16
    .unused             db  0
    .count              dw  1
    .target_offset      dw  0
    .target_segment     dw  0
    .lba_address_low    dd  0
    .lba_address_high   dd  0


read_sectors:

; ********************************************
;     Reads multiple LBA addressed sectors
; ********************************************

; IN:
; EAX = LBA starting sector
; DL = Drive number
; ES = Buffer segment
; EBX = Buffer offset
; CX = Sectors count

; OUT:
; Carry if error

push eax									; Save GPRs
push ebx
push ecx
push edx
push esi
push edi

.loop:

push es
push ebx

mov bx, 0x7000							; Load in a temp buffer
mov es, bx
xor bx, bx

call read_sector						; Read sector

pop ebx
pop es

jc .done								; If carry exit with flag

push ds

mov si, 0x7000
mov ds, si
mov edi, ebx
xor esi, esi

push ecx
mov ecx, 512
a32 o32 rep movsb
pop ecx

pop ds

inc eax									; Increment sector
add ebx, 512							; Add 512 to the buffer

loop .loop								; Loop!

.done:
pop edi
pop esi
pop edx
pop ecx									; Restore GPRs
pop ebx
pop eax
ret										; Exit routine
