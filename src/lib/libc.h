#ifndef __LIBC_H__
#define __LIBC_H__

#include <stddef.h>

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
char *strchrnul(const char *, int);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
char *strtok(char *str, const char *delimiter);

#endif
