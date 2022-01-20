#ifndef __STDDEF_H__
#define __STDDEF_H__

typedef __SIZE_TYPE__ size_t;

#ifdef NULL
#undef NULL
#endif

#define NULL ((void *)0)

#define offsetof(s, m) __builtin_offsetof(s, m)

#endif
