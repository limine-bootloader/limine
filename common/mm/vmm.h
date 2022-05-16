#ifndef __MM__VMM_H__
#define __MM__VMM_H__

#include <stdint.h>
#include <stdbool.h>

#define VM_READ  ((uint64_t)1 << 0)
#define VM_WRITE ((uint64_t)1 << 1)
#define VM_EXEC  ((uint64_t)1 << 2)

typedef struct {
#if defined (__i386__) || defined (__x86_64__)
    int   levels;
    void *top_level;
#elif defined (__aarch64__)
    void *ttbr0;
    void *ttbr1;
#endif
} pagemap_t;


void vmm_assert_nx(void);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, uint64_t size);

uint64_t get_page_size();

#if defined (__i386__) || defined (__x86_64__)
pagemap_t new_pagemap(int lv);
#elif defined (__aarch64__)
pagemap_t new_pagemap(void);
#endif

#endif
