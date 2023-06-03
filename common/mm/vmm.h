#ifndef __MM__VMM_H__
#define __MM__VMM_H__

#include <stdint.h>
#include <stdbool.h>

#if defined (__x86_64__) || defined (__i386__)

#define VMM_FLAG_WRITE   ((uint64_t)1 << 1)
#define VMM_FLAG_NOEXEC  ((uint64_t)1 << 63)
#define VMM_FLAG_FB      ((uint64_t)0)

#define VMM_MAX_LEVEL 2

#define PAGING_MODE_X86_64_4LVL 0
#define PAGING_MODE_X86_64_5LVL 1

static inline uint64_t paging_mode_higher_half(int paging_mode) {
    if (paging_mode == PAGING_MODE_X86_64_5LVL) {
        return 0xff00000000000000;
    } else {
        return 0xffff800000000000;
    }
}

typedef struct {
    int   levels;
    void *top_level;
} pagemap_t;

enum page_size {
    Size4KiB,
    Size2MiB,
    Size1GiB
};

pagemap_t new_pagemap(int lv);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size page_size);

#elif defined (__aarch64__)

// We use fake flags here because these don't properly map onto the
// aarch64 flags.
#define VMM_FLAG_WRITE   ((uint64_t)1 << 0)
#define VMM_FLAG_NOEXEC  ((uint64_t)1 << 1)
#define VMM_FLAG_FB      ((uint64_t)1 << 2)

#define VMM_MAX_LEVEL 2

#define PAGING_MODE_AARCH64_4LVL 0
#define PAGING_MODE_AARCH64_5LVL 1

#define paging_mode_va_bits(mode) ((mode) ? 57 : 48)

static inline uint64_t paging_mode_higher_half(int paging_mode) {
    if (paging_mode == PAGING_MODE_AARCH64_5LVL) {
        return 0xff00000000000000;
    } else {
        return 0xffff800000000000;
    }
}

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

#elif defined (__riscv64)

// We use fake flags here because these don't properly map onto the
// RISC-V flags.
#define VMM_FLAG_WRITE   ((uint64_t)1 << 0)
#define VMM_FLAG_NOEXEC  ((uint64_t)1 << 1)
#define VMM_FLAG_FB      ((uint64_t)1 << 2)

#define VMM_MAX_LEVEL 4

#define PAGING_MODE_RISCV_SV39 8
#define PAGING_MODE_RISCV_SV48 9
#define PAGING_MODE_RISCV_SV57 10

enum page_size {
    Size4KiB,
    Size2MiB,
    Size1GiB,
    Size512GiB,
    Size256TiB
};

typedef struct {
    enum page_size max_page_size;
    int            paging_mode;
    void          *top_level;
} pagemap_t;

uint64_t paging_mode_higher_half(int paging_mode);
int vmm_max_paging_mode(void);
pagemap_t new_pagemap(int paging_mode);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size page_size);

#else
#error Unknown architecture
#endif

int vmm_max_paging_mode(void);

#endif
