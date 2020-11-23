#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/pmm.h>
#include <sys/e820.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>

#define PAGE_SIZE   4096
#define MEMMAP_BASE ((size_t)0x100000)
#define MEMMAP_MAX_ENTRIES 256

static struct e820_entry_t memmap[MEMMAP_MAX_ENTRIES];
static size_t memmap_entries = 0;

static const char *memmap_type(uint32_t type) {
    switch (type) {
        case MEMMAP_USABLE:
            return "Usable RAM";
        case MEMMAP_RESERVED:
            return "Reserved";
        case MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI reclaimable";
        case MEMMAP_ACPI_NVS:
            return "ACPI NVS";
        case MEMMAP_BAD_MEMORY:
            return "Bad memory";
        case MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case MEMMAP_KERNEL_AND_MODULES:
            return "Kernel/Modules";
        default:
            return "???";
    }
}

void print_memmap(struct e820_entry_t *mm, size_t size) {
    for (size_t i = 0; i < size; i++) {
        print("[%X -> %X] : %X  <%s>\n",
              mm[i].base,
              mm[i].base + mm[i].length,
              mm[i].length,
              memmap_type(mm[i].type));
    }
}

static bool align_entry(uint64_t *base, uint64_t *length) {
    if (*length < PAGE_SIZE)
        return false;

    uint64_t orig_base = *base;

    *base = ALIGN_UP(*base, PAGE_SIZE);

    *length -= (*base - orig_base);
    *length =  ALIGN_DOWN(*length, PAGE_SIZE);

    if (!length)
        return false;

    uint64_t top = *base + *length;

    if (*base < MEMMAP_BASE) {
        if (top > MEMMAP_BASE) {
            *length -= MEMMAP_BASE - *base;
            *base    = MEMMAP_BASE;
        } else {
            return false;
        }
    }

    return true;
}

static void sanitise_entries(bool align_entries) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE)
            continue;

        // Check if the entry overlaps other entries
        for (size_t j = 0; j < memmap_entries; j++) {
            if (j == i)
                continue;

            uint64_t base   = memmap[i].base;
            uint64_t length = memmap[i].length;
            uint64_t top    = base + length;

            uint64_t res_base   = memmap[j].base;
            uint64_t res_length = memmap[j].length;
            uint64_t res_top    = res_base + res_length;

            // TODO actually handle splitting off usable chunks
            if ( (res_base >= base && res_base < top)
              && (res_top  >= base && res_top  < top) ) {
                panic("A non-usable e820 entry is inside a usable section.");
            }

            if (res_base >= base && res_base < top) {
                top = res_base;
            }

            if (res_top  >= base && res_top  < top) {
                base = res_top;
            }

            memmap[i].base   = base;
            memmap[i].length = top - base;
        }

        if (!memmap[i].length
         || (align_entries && !align_entry(&memmap[i].base, &memmap[i].length))) {
            // Eradicate from memmap
            for (size_t j = i; j < memmap_entries - 1; j++) {
                memmap[j] = memmap[j+1];
            }
            memmap_entries--;
            i--;
        }
    }

    // Sort the entries
    for (size_t p = 0; p < memmap_entries - 1; p++) {
        uint64_t min = memmap[p].base;
        size_t min_index = p;
        for (size_t i = p; i < memmap_entries; i++) {
            if (memmap[i].base < min) {
                min = memmap[i].base;
                min_index = i;
            }
        }
        struct e820_entry_t min_e = memmap[min_index];
        memmap[min_index] = memmap[p];
        memmap[p] = min_e;
    }

    // Merge contiguous bootloader-reclaimable entries
    for (size_t i = 0; i < memmap_entries - 1; i++) {
        if (memmap[i].type != MEMMAP_BOOTLOADER_RECLAIMABLE)
            continue;

        if (memmap[i+1].type == MEMMAP_BOOTLOADER_RECLAIMABLE
         && memmap[i+1].base == memmap[i].base + memmap[i].length) {
            memmap[i].length += memmap[i+1].length;

            // Eradicate from memmap
            for (size_t j = i+1; j < memmap_entries - 1; j++) {
                memmap[j] = memmap[j+1];
            }
            memmap_entries--;
            i--;
        }
    }
}

struct e820_entry_t *get_memmap(size_t *entries) {
    sanitise_entries(true);

    *entries = memmap_entries;

    return memmap;
}

void init_memmap(void) {
    for (size_t i = 0; i < e820_entries; i++) {
        if (memmap_entries == MEMMAP_MAX_ENTRIES) {
            panic("Memory map exhausted.");
        }

        memmap[memmap_entries++] = e820_map[i];
    }

    sanitise_entries(false);
}

void *ext_mem_alloc(size_t count) {
    return ext_mem_alloc_type(count, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

void *ext_mem_alloc_aligned(size_t count, size_t alignment) {
    return ext_mem_alloc_aligned_type(count, alignment, MEMMAP_BOOTLOADER_RECLAIMABLE);
}

void *ext_mem_alloc_type(size_t count, uint32_t type) {
    return ext_mem_alloc_aligned_type(count, 4, type);
}

// Allocate memory top down, hopefully without bumping into kernel or modules
void *ext_mem_alloc_aligned_type(size_t count, size_t alignment, uint32_t type) {
    for (int i = memmap_entries - 1; i >= 0; i--) {
        if (memmap[i].type != 1)
            continue;

        int64_t entry_base = (int64_t)(memmap[i].base);
        int64_t entry_top  = (int64_t)(memmap[i].base + memmap[i].length);

        // Let's make sure the entry is not > 4GiB
        if (entry_base >= 0x100000000 || entry_top >= 0x100000000) {
            // Theoretically there could be an entry which crosses the 4GiB
            // boundary, but realistically this does not happen as far as I
            // have seen. Let's just discard the entry.
            continue;
        }

        int64_t alloc_base = ALIGN_DOWN(entry_top - (int64_t)count, alignment);

        // This entry is too small for us.
        if (alloc_base < entry_base)
            continue;

        // We now reserve the range we need.
        int64_t aligned_length = entry_top - alloc_base;
        memmap_alloc_range((uint64_t)alloc_base, (uint64_t)aligned_length, type);

        void *ret = (void *)(size_t)alloc_base;

        // Zero out allocated space
        memset(ret, 0, count);

        sanitise_entries(false);

        return ret;
    }

    panic("High memory allocator: Out of memory");
}

void memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type) {
    uint64_t top = base + length;

    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != 1)
            continue;

        uint64_t entry_base = memmap[i].base;
        uint64_t entry_top  = memmap[i].base + memmap[i].length;
        if (base >= entry_base && base <  entry_top &&
            top  >= entry_base && top  <= entry_top) {

            memmap[i].length = base - entry_base;

            if (memmap[i].length == 0) {
                // Eradicate from memmap
                for (size_t j = i; j < memmap_entries - 1; j++) {
                    memmap[j] = memmap[j+1];
                }
                memmap_entries--;
            }

            if (memmap_entries >= MEMMAP_MAX_ENTRIES) {
                panic("Memory map exhausted.");
            }
            struct e820_entry_t *target = &memmap[memmap_entries];

            target->length = entry_top - top;

            if (target->length != 0) {
                target->base = top;
                target->type = 1;

                memmap_entries++;
            }

            if (memmap_entries >= MEMMAP_MAX_ENTRIES) {
                panic("Memory map exhausted.");
            }
            target = &memmap[memmap_entries++];

            target->type   = type;
            target->base   = base;
            target->length = length;

            return;
        }
    }

    panic("Out of memory");
}

extern symbol bss_end;
static size_t bump_allocator_base = (size_t)bss_end;
static size_t bump_allocator_limit = 0;

void *conv_mem_alloc(size_t count) {
    return conv_mem_alloc_aligned(count, 4);
}

void *conv_mem_alloc_aligned(size_t count, size_t alignment) {
    if (!bump_allocator_limit) {
        // The balloc limit is the beginning of the GDT
        struct {
            uint16_t limit;
            uint32_t ptr;
        } __attribute__((packed)) gdtr;
        asm volatile ("sgdt %0" :: "m"(gdtr) : "memory");
        bump_allocator_limit = gdtr.ptr;
    }

    size_t new_base = ALIGN_UP(bump_allocator_base, alignment);
    void *ret = (void *)new_base;
    new_base += count;
    if (new_base >= bump_allocator_limit)
        panic("Memory allocation failed");
    bump_allocator_base = new_base;

    // Zero out allocated space
    memset(ret, 0, count);

    return ret;
}
