#include <stdint.h>
#include <stddef.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/blib.h>
#include <lib/print.h>

typedef uint64_t pt_entry_t;

void vmm_assert_nx(void) {
}

pagemap_t new_pagemap(void) {
    pagemap_t pagemap;
    pagemap.ttbr0 = ext_mem_alloc_type_aligned(get_page_size(), MEMMAP_BOOTLOADER_RECLAIMABLE, get_page_size());
    pagemap.ttbr1 = ext_mem_alloc_type_aligned(get_page_size(), MEMMAP_BOOTLOADER_RECLAIMABLE, get_page_size());
    return pagemap;
}


void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, uint64_t size) {
    (void)pagemap;
    (void)virt_addr;
    (void)phys_addr;
    (void)flags;
    (void)size;

    void* root = pagemap.ttbr0;
    if (virt_addr > 0x8000000000000000) root = pagemap.ttbr1;

    uint64_t levels = get_page_size() == 0x10000 ? 3 : 4;
    
    uint64_t base_bits = __builtin_ctz(get_page_size());
    print("map_page(%X -> %X + %X, flags=%x)\n", virt_addr, phys_addr, size, flags);

    while (size > 0) {
        void* current_table = root;
        uint64_t level = levels - 1;
        uint64_t current_step_size = get_page_size() << ((base_bits - 3) * (levels - 1));
        
        while (true) {
            uint64_t shift_bits = base_bits + (base_bits - 3) * level;
            uint64_t index = (virt_addr >> shift_bits) & ((1ULL << (base_bits - 3)) - 1);


            uint64_t* entry = &((uint64_t*)current_table)[index];
            

            if (!(*entry & 1)) {
                // empty
                do {
                    if (current_step_size > 0x40000000) {
                        break; // sabaton does this too
                    }
                    if (size < current_step_size) {
                        break; // we would map too much
                    }
                    uint64_t mask = current_step_size - 1;
                    if (virt_addr & mask) break; // unaligned virt address
                    if (phys_addr & mask) break; // unaligned phys addr

                    // work out the bits
                    uint64_t bits = 1 | (1 << 5) | (1 << 10);
                    bits |= 0 << 2 | 2 << 8 | 1 << 11;
                    if (get_page_size() < 0x10000 && level == 0) bits |= 2;
                    if (!(flags & VM_WRITE)) bits |= 1 << 7; // forbid write (?)
                    if (!(flags & VM_EXEC)) bits |= 1ULL << 54; // privileged execute never

                    *entry = bits | phys_addr;
                    goto mapped;
                } while(0);
            } else if (level == 0 || (*entry & 2) == 0) {
                current_step_size = get_page_size();
                goto mapped;
            }

            if (*entry == 0) {
                void* table = ext_mem_alloc_type_aligned(get_page_size(), MEMMAP_BOOTLOADER_RECLAIMABLE, get_page_size());
                *entry = ((uint64_t)table) | 3 | (1ULL << 63);
                current_table = table;
            } else {
                current_table = (void*)(*entry & 0x0000fffffffff000);
            }
            current_step_size >>= (base_bits - 3);
            level -= 1;
        }

    mapped: 
        virt_addr += current_step_size;
        phys_addr += current_step_size;
        size -= current_step_size;
    }
}

uint64_t get_page_size() {
    uint64_t aa64mmfr0;
    asm (
        "MRS %[aa64mmfr0], ID_AA64MMFR0_EL1\n"
        : [aa64mmfr0] "=r"(aa64mmfr0)
    );

    uint64_t psz;

    if (((aa64mmfr0 >> 28) & 0x0F) == 0b0000) {
        psz = 0x1000;
    } else if (((aa64mmfr0 >> 20) & 0x0F) == 0b0001) {
        psz = 0x4000;
    } else if (((aa64mmfr0 >> 24) & 0x0F) == 0b0000) {
        psz = 0x10000;
    } else {
        panic(false, "Unknown page size!\n");
    }

    return psz;
}
