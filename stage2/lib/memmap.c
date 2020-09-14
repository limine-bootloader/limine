#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/memmap.h>
#include <lib/e820.h>
#include <lib/blib.h>
#include <lib/print.h>

#define PAGE_SIZE   4096
#define MEMMAP_BASE ((size_t)0x100000)
#define MEMMAP_MAX_ENTRIES 256

#define MEMMAP_USABLE                 1
#define MEMMAP_RESERVED               2
#define MEMMAP_ACPI_RECLAIMABLE       3
#define MEMMAP_ACPI_NVS               4
#define MEMMAP_BAD_MEMORY             5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_KERNEL_AND_MODULES     0x1001

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

static int align_entry(uint64_t *base, uint64_t *length) {
    if (*length < PAGE_SIZE)
        return -1;

    uint64_t orig_base = *base;

    *base = ALIGN_UP(*base, PAGE_SIZE);

    *length -= (*base - orig_base);
    *length =  ALIGN_DOWN(*length, PAGE_SIZE);

    if (!length)
        return -1;

    uint64_t top = *base + *length;

    if (*base < MEMMAP_BASE) {
        if (top > MEMMAP_BASE) {
            *length -= MEMMAP_BASE - *base;
            *base    = MEMMAP_BASE;
        } else {
            return -1;
        }
    }

    return 0;
}

static void sanitise_entries(void) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != 1)
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

        if (!memmap[i].length || align_entry(&memmap[i].base, &memmap[i].length)) {
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
}

struct e820_entry_t *get_memmap(size_t *entries) {
    sanitise_entries();

    *entries = memmap_entries;

    print("Memory map requested. Current layout:\n");
    print_memmap(memmap, memmap_entries);

    return memmap;
}

void init_memmap(void) {
    for (size_t i = 0; i < e820_entries; i++) {
        if (memmap_entries == MEMMAP_MAX_ENTRIES) {
            panic("Memory map exhausted.");
        }

        memmap[memmap_entries++] = e820_map[i];
    }

    sanitise_entries();
}

static size_t ext_mem_balloc_base = 0x100000;

void *ext_mem_balloc(size_t count) {
    return ext_mem_balloc_aligned(count, 4);
}

// TODO: this basically only works for the 1st extended memory entry in the
//       memmap and allocates until the first hole following it. Fix that.
void *ext_mem_balloc_aligned(size_t count, size_t alignment) {
    uint64_t base = ALIGN_UP(ext_mem_balloc_base, alignment);
    uint64_t top  = base + count;

    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != 1)
            continue;

        uint64_t entry_base = memmap[i].base;
        uint64_t entry_top  = memmap[i].base + memmap[i].length;
        if (base >= entry_base && base < entry_top &&
            top  >= entry_base && top  < entry_top) {
            ext_mem_balloc_base = base + count;
            return (void *)(size_t)base;
        }
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
        if (base >= entry_base && base < entry_top &&
            top  >= entry_base && top  < entry_top) {

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
