#include <stddef.h>
#include <stdint.h>
#include <lib/libc.h>
#include <lib/asm.h>

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - 0x20;
    }
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 0x20;
    }
    return c;
}

__attribute__((naked))
void *memcpy(void *dest, const void *src, size_t n) {
    ASM_BASIC(
        "push esi\n\t"
        "push edi\n\t"
        "mov eax, dword ptr [esp+12]\n\t"
        "mov edi, eax\n\t"
        "mov esi, dword ptr [esp+16]\n\t"
        "mov ecx, dword ptr [esp+20]\n\t"
        "rep movsb\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked))
void *memset(void *s, int c, size_t n) {
    ASM_BASIC(
        "push edi\n\t"
        "mov edx, dword ptr [esp+8]\n\t"
        "mov edi, edx\n\t"
        "mov eax, dword ptr [esp+12]\n\t"
        "mov ecx, dword ptr [esp+16]\n\t"
        "rep stosb\n\t"
        "mov eax, edx\n\t"
        "pop edi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked))
void *memmove(void *dest, const void *src, size_t n) {
    ASM_BASIC(
        "push esi\n\t"
        "push edi\n\t"
        "mov eax, dword ptr [esp+12]\n\t"
        "mov edi, eax\n\t"
        "mov esi, dword ptr [esp+16]\n\t"
        "mov ecx, dword ptr [esp+20]\n\t"

        "cmp edi, esi\n\t"
        "ja 1f\n\t"

        "rep movsb\n\t"
        "jmp 2f\n\t"

        "1:\n\t"
        "lea edi, [edi+ecx-1]\n\t"
        "lea esi, [esi+ecx-1]\n\t"
        "std\n\t"
        "rep movsb\n\t"
        "cld\n\t"

        "2:\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "ret\n\t"
    );
}

__attribute__((naked))
int memcmp(const void *s1, const void *s2, size_t n) {
    ASM_BASIC(
        "push esi\n\t"
        "push edi\n\t"
        "mov edi, dword ptr [esp+12]\n\t"
        "mov esi, dword ptr [esp+16]\n\t"
        "mov ecx, dword ptr [esp+20]\n\t"
        "repe cmpsb\n\t"
        "jecxz 1f\n\t"
        "mov al, byte ptr [edi-1]\n\t"
        "sub al, byte ptr [esi-1]\n\t"
        "movsx eax, al\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "mov eax, ecx\n\t"
        "2:\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "ret\n\t"
    );
}

char *strcpy(char *dest, const char *src) {
    size_t i;

    for (i = 0; src[i]; i++)
        dest[i] = src[i];

    dest[i] = 0;

    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = 0;

    return dest;
}

int strcmp(const char *s1, const char *s2) {
    for (size_t i = 0; ; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }

    return 0;
}

size_t strlen(const char *str) {
    size_t len;

    for (len = 0; str[len]; len++);

    return len;
}
