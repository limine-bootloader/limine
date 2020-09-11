#include <stdint.h>
#include <stddef.h>
#include <mm/vmm64.h>
#include <lib/blib.h>

#define PT_SIZE ((uint64_t)0x1000)

typedef uint64_t pt_entry_t;

static pt_entry_t *get_next_level(pt_entry_t *current_level, size_t entry) {
    pt_entry_t *ret;

    if (current_level[entry] & 0x1) {
        // Present flag set
        ret = (pt_entry_t *)(current_level[entry] & ~((pt_entry_t)0xfff));
    } else {
        // Allocate a table for the next level
        ret = balloc_aligned(PT_SIZE, PT_SIZE);
        // Present + writable + user (0b111)
        current_level[entry] = (pt_entry_t)ret | 0b111;
    }

    return ret;
}

pagemap_t new_pagemap(void) {
    return (pagemap_t)(size_t)balloc_aligned(PT_SIZE, PT_SIZE);
}

void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    // Calculate the indices in the various tables using the virtual address
    size_t pml4_entry = (virt_addr & ((uint64_t)0x1ff << 39)) >> 39;
    size_t pml3_entry = (virt_addr & ((uint64_t)0x1ff << 30)) >> 30;
    size_t pml2_entry = (virt_addr & ((uint64_t)0x1ff << 21)) >> 21;

    pt_entry_t *pml4 = (pt_entry_t *)(size_t)pagemap;
    pt_entry_t *pml3 = get_next_level(pml4, pml4_entry);
    pt_entry_t *pml2 = get_next_level(pml3, pml3_entry);

    // Set the entry as present and point it to the passed physical address
    // Also set the specified flags
    // We only use 2MiB pages else we would not have enough space
    pml2[pml2_entry] = (pt_entry_t)(phys_addr | flags | (1 << 7));
}
