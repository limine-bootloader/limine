#ifndef __LIB__LIBC_H__
#define __LIB__LIBC_H__

#include <stddef.h>

void* memset(void*, int, size_t);
void* memcpy(void*, const void*, size_t);
int memcmp(const void*, const void*, size_t);
void* memmove(void*, const void*, size_t);
char* strcpy(char*, const char*);
char* strncpy(char*, const char*, size_t);
size_t strlen(const char*);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);

/// \brief Convert the given character to uppercase.
///
/// \note Only supports ASCII.
///
/// \param ch Character to be converted.
///
/// \return Uppercase version of ch.
int toupper(int ch);

/// \brief Convert the given character to lowercase.
///
/// \note Only supports ASCII.
///
/// \param ch Character to be converted.
///
/// \return Lowercase version of ch.
int tolower(int ch);

#endif
