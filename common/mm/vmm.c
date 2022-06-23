#include <stdint.h>
#include <stddef.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <sys/cpu.h>

#define PT_SIZE ((uint64_t)0x1000)

typedef uint64_t pt_entry_t;

#if defined (__x86_64__) || defined (__i386__)

void vmm_assert_nx(void) {
    uint32_t a, b, c, d;
    if (!cpuid(0x80000001, 0, &a, &b, &c, &d) || !(d & (1 << 20))) {
        panic(false, "vmm: NX functionality not available on this CPU.");
    }
}

static pt_entry_t *get_next_level(pt_entry_t *current_level, size_t entry) {
    pt_entry_t *ret;

    if (current_level[entry] & 0x1) {
        // Present flag set
        ret = (pt_entry_t *)(size_t)(current_level[entry] & ~((pt_entry_t)0xfff));
    } else {
        // Allocate a table for the next level
        ret = ext_mem_alloc(PT_SIZE);
        // Present + writable + user (0b111)
        current_level[entry] = (pt_entry_t)(size_t)ret | 0b111;
    }

    return ret;
}

pagemap_t new_pagemap(int lv) {
    pagemap_t pagemap;
    pagemap.levels    = lv;
    pagemap.top_level = ext_mem_alloc(PT_SIZE);
    return pagemap;
}

static bool is_1gib_page_supported(void) {
    // Cache the cpuid result :^)
    static bool CACHE_INIT = false;
    static bool CACHE = false;

    if (!CACHE_INIT) {
        // Check if 1GiB pages are supported:
        uint32_t eax, ebx, ecx, edx;

        CACHE = cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) && ((edx & 1 << 26) == 1 << 26);
        CACHE_INIT = true;

        printv("paging: 1GiB pages are %s!\n", CACHE ? "supported" : "not supported");
    }

    return CACHE;
}

void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size pg_size) {
    // Calculate the indices in the various tables using the virtual address
    size_t pml5_entry = (virt_addr & ((uint64_t)0x1ff << 48)) >> 48;
    size_t pml4_entry = (virt_addr & ((uint64_t)0x1ff << 39)) >> 39;
    size_t pml3_entry = (virt_addr & ((uint64_t)0x1ff << 30)) >> 30;
    size_t pml2_entry = (virt_addr & ((uint64_t)0x1ff << 21)) >> 21;
    size_t pml1_entry = (virt_addr & ((uint64_t)0x1ff << 12)) >> 12;

    pt_entry_t *pml5, *pml4, *pml3, *pml2, *pml1;

    flags |= 1; // Always present

    // Paging levels
    switch (pagemap.levels) {
        case 5:
            pml5 = pagemap.top_level;
            goto level5;
        case 4:
            pml4 = pagemap.top_level;
            goto level4;
        default:
            __builtin_unreachable();
    }

level5:
    pml4 = get_next_level(pml5, pml5_entry);
level4:
    pml3 = get_next_level(pml4, pml4_entry);

    if (pg_size == Size1GiB) {
        // Check if 1GiB pages are avaliable.
        if (is_1gib_page_supported()) {
            pml3[pml3_entry] = (pt_entry_t)(phys_addr | flags | (1 << 7));
            return;
        } else {
            // If 1GiB pages are not supported then emulate it by splitting them into
            // 2MiB pages.
            for (uint64_t i = 0; i < 0x40000000; i += 0x200000) {
                map_page(pagemap, virt_addr + i, phys_addr + i, flags, Size2MiB);
            }
        }
    }

    pml2 = get_next_level(pml3, pml3_entry);

    if (pg_size == Size2MiB) {
        pml2[pml2_entry] = (pt_entry_t)(phys_addr | flags | (1 << 7));
        return;
    }

    pml1 = get_next_level(pml2, pml2_entry);

    pml1[pml1_entry] = (pt_entry_t)(phys_addr | flags);
}

#elif defined (__aarch64__)

// Here we operate under the assumption that 4K pages are supported by the CPU.
// This appears to be guaranteed by UEFI, as section 2.3.6 "AArch64 Platforms"
// states that the primary processor core configuration includes 4K translation
// granules (TCR_EL1.TG0 = 0).
// Support for 4K pages also implies 2M, 1G and 512G blocks.

// Sanity check that 4K pages are supported.
void vmm_assert_4k_pages(void) {
    uint64_t aa64mmfr0;
    asm volatile ("mrs %0, id_aa64mmfr0_el1" : "=r"(aa64mmfr0));

    if (((aa64mmfr0 >> 28) & 0b1111) == 0b1111) {
        panic(false, "vmm: CPU does not support 4K pages, please make a bug report about this.");
    }
}

#define PT_FLAG_VALID    ((uint64_t)1 << 0)
#define PT_FLAG_TABLE    ((uint64_t)1 << 1)
#define PT_FLAG_4K_PAGE  ((uint64_t)1 << 1)
#define PT_FLAG_BLOCK    ((uint64_t)0 << 1)
#define PT_FLAG_USER     ((uint64_t)1 << 6)
#define PT_FLAG_READONLY ((uint64_t)1 << 7)
#define PT_FLAG_INNER_SH ((uint64_t)3 << 8)
#define PT_FLAG_ACCESS   ((uint64_t)1 << 10)
#define PT_FLAG_XN       ((uint64_t)1 << 54)
#define PT_FLAG_WB       ((uint64_t)0 << 2)

static pt_entry_t *get_next_level(pt_entry_t *current_level, size_t entry) {
    pt_entry_t *ret;

    if (current_level[entry] & PT_FLAG_VALID) {
        // Present flag set
        ret = (pt_entry_t *)(size_t)(current_level[entry] & ~((pt_entry_t)0xfff));
    } else {
        // Allocate a table for the next level
        ret = ext_mem_alloc(PT_SIZE);
        current_level[entry] = (pt_entry_t)(size_t)ret | PT_FLAG_VALID
                | PT_FLAG_TABLE;
    }

    return ret;
}

pagemap_t new_pagemap(int lv) {
    pagemap_t pagemap;
    pagemap.levels       = lv;
    pagemap.top_level[0] = ext_mem_alloc(PT_SIZE);
    pagemap.top_level[1] = ext_mem_alloc(PT_SIZE);
    return pagemap;
}

void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, enum page_size pg_size) {
    // Calculate the indices in the various tables using the virtual address
    size_t pml5_entry = (virt_addr & ((uint64_t)0xf << 48)) >> 48;
    size_t pml4_entry = (virt_addr & ((uint64_t)0x1ff << 39)) >> 39;
    size_t pml3_entry = (virt_addr & ((uint64_t)0x1ff << 30)) >> 30;
    size_t pml2_entry = (virt_addr & ((uint64_t)0x1ff << 21)) >> 21;
    size_t pml1_entry = (virt_addr & ((uint64_t)0x1ff << 12)) >> 12;

    pt_entry_t *pml5, *pml4, *pml3, *pml2, *pml1;

    bool is_higher_half = virt_addr & ((uint64_t)1 << 63);

    uint64_t real_flags = PT_FLAG_VALID | PT_FLAG_INNER_SH | PT_FLAG_ACCESS | PT_FLAG_WB;
    if (!(flags & VMM_FLAG_WRITE))
        real_flags |= PT_FLAG_READONLY;
    if (flags & VMM_FLAG_NOEXEC)
        real_flags |= PT_FLAG_XN;

    // Paging levels
    switch (pagemap.levels) {
        case 5:
            pml5 = pagemap.top_level[is_higher_half];
            goto level5;
        case 4:
            pml4 = pagemap.top_level[is_higher_half];
            goto level4;
        default:
            __builtin_unreachable();
    }

level5:
    pml4 = get_next_level(pml5, pml5_entry);
level4:
    pml3 = get_next_level(pml4, pml4_entry);

    if (pg_size == Size1GiB) {
        pml3[pml3_entry] = (pt_entry_t)(phys_addr | real_flags | PT_FLAG_BLOCK);
        return;
    }

    pml2 = get_next_level(pml3, pml3_entry);

    if (pg_size == Size2MiB) {
        pml2[pml2_entry] = (pt_entry_t)(phys_addr | real_flags | PT_FLAG_BLOCK);
        return;
    }

    pml1 = get_next_level(pml2, pml2_entry);

    pml1[pml1_entry] = (pt_entry_t)(phys_addr | real_flags | PT_FLAG_4K_PAGE);
}

#else
#error Unknown architecture
#endif
