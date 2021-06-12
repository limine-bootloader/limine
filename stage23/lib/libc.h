#ifndef __LIB__LIBC_H__
#define __LIB__LIBC_H__

#include <stddef.h>

int toupper(int c);
int tolower(int c);

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memmove(void *, const void *, size_t);

char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strcasecmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int inet_pton(const char *src, void *dst);

#endif
