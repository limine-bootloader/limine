#ifndef __MM__VMM_H__
#define __MM__VMM_H__

#include <stdint.h>
#include <stdbool.h>

#if defined (__x86_64__) || defined (__i386__)

#define VMM_FLAG_WRITE   ((uint64_t)1 << 1)
#define VMM_FLAG_NOEXEC  ((uint64_t)1 << 63)

typedef struct {
    int   levels;
    void *top_level;
} pagemap_t;

enum page_size {
    Size4KiB,
    Size2MiB,
    Size1GiB
};

void vmm_assert_nx(void);
pagemap_t new_pagemap(int lv);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size page_size);

#elif defined (__aarch64__)

// We use fake flags here because these don't properly map onto the
// aarch64 flags.
#define VMM_FLAG_WRITE   ((uint64_t)1 << 0)
#define VMM_FLAG_NOEXEC  ((uint64_t)1 << 1)

typedef struct {
    int   levels;
    void *top_level[2];
} pagemap_t;

enum page_size {
    Size4KiB,
    Size2MiB,
    Size1GiB
};

void vmm_assert_4k_pages(void);
pagemap_t new_pagemap(int lv);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size page_size);

#else
#error Unknown architecture
#endif

#endif
