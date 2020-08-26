#ifndef __LIB__ASM_H__
#define __LIB__ASM_H__

#define ASM(body, ...) asm volatile (".intel_syntax noprefix\n\t" body ".att_syntax prefix" : __VA_ARGS__)
#define ASM_BASIC(body) asm (".intel_syntax noprefix\n\t" body ".att_syntax prefix")

#define FARJMP16(seg, off) \
    ".byte 0xea\n\t" \
    ".2byte " off "\n\t" \
    ".2byte " seg "\n\t" \

#define FARJMP32(seg, off) \
    ".byte 0xea\n\t" \
    ".4byte " off "\n\t" \
    ".2byte " seg "\n\t" \

#endif
