#include <stdint.h>
#include <stddef.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/blib.h>

#define PT_SIZE ((uint64_t)0x1000)

typedef uint64_t pt_entry_t;

stage3_text
static pt_entry_t *get_next_level(pt_entry_t *current_level, size_t entry) {
    pt_entry_t *ret;

    if (current_level[entry] & 0x1) {
        // Present flag set
        ret = (pt_entry_t *)(size_t)(current_level[entry] & ~((pt_entry_t)0xfff));
    } else {
        // Allocate a table for the next level
        ret = ext_mem_alloc_aligned(PT_SIZE, PT_SIZE);
        // Present + writable + user (0b111)
        current_level[entry] = (pt_entry_t)(size_t)ret | 0b111;
    }

    return ret;
}

stage3_text
pagemap_t new_pagemap(int lv) {
    pagemap_t pagemap;
    pagemap.levels    = lv;
    pagemap.top_level = ext_mem_alloc_aligned(PT_SIZE, PT_SIZE);
    return pagemap;
}

stage3_text
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    // Calculate the indices in the various tables using the virtual address
    size_t pml5_entry = (virt_addr & ((uint64_t)0x1ff << 48)) >> 48;
    size_t pml4_entry = (virt_addr & ((uint64_t)0x1ff << 39)) >> 39;
    size_t pml3_entry = (virt_addr & ((uint64_t)0x1ff << 30)) >> 30;
    size_t pml2_entry = (virt_addr & ((uint64_t)0x1ff << 21)) >> 21;

    pt_entry_t *pml5, *pml4, *pml3, *pml2;

    // Paging levels
    switch (pagemap.levels) {
        case 5:
            pml5 = pagemap.top_level;
            goto level5;
        case 4:
            pml4 = pagemap.top_level;
            goto level4;
        default:
            panic("");
    }

level5:
    pml4 = get_next_level(pml5, pml5_entry);
level4:
    pml3 = get_next_level(pml4, pml4_entry);
    pml2 = get_next_level(pml3, pml3_entry);

    // Set the entry as present and point it to the passed physical address
    // Also set the specified flags
    // We only use 2MiB pages else we would not have enough space
    pml2[pml2_entry] = (pt_entry_t)(phys_addr | flags | (1 << 7));
}
