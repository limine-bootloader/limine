#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/memmap.h>
#include <lib/e820.h>
#include <lib/blib.h>

#define PAGE_SIZE   4096
#define MEMMAP_BASE ((size_t)0x100000)
#define MEMMAP_MAX_ENTRIES 256

static struct e820_entry_t memmap[MEMMAP_MAX_ENTRIES];
static size_t memmap_entries = 0;

static void align_entry_down(uint64_t *base, uint64_t *length) {
    uint64_t orig_base = *base;

    *base   = ALIGN_UP(*base, PAGE_SIZE);
    *length = *length - (*base - orig_base);
    *length = ALIGN_DOWN(*length, PAGE_SIZE);
}

static int align_entry_with_base(uint64_t *base, uint64_t *length) {
    align_entry_down(base, length);

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

static void memmap_align_free_entries(void) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != 1)
            continue;

        align_entry_down(&memmap[i].base, &memmap[i].length);

        if (!memmap[i].length) {
            // Eradicate from memmap
            for (size_t j = i; j < memmap_entries - 1; j++) {
                memmap[j] = memmap[j+1];
            }
            memmap_entries--;
        }
    }
}

struct e820_entry_t *get_memmap(size_t *entries) {
    memmap_align_free_entries();

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
        memmap[min_index] = memmap[0];
        memmap[p] = min_e;
    }

    *entries = memmap_entries;
    return memmap;
}

void init_memmap(void) {
    for (size_t i = 0; i < e820_entries; i++) {
        if (memmap_entries == MEMMAP_MAX_ENTRIES) {
            panic("Memory map exhausted.");
        }

        if (e820_map[i].type != 1) {
            memmap[memmap_entries++] = e820_map[i];
            continue;
        }

        uint64_t base   = e820_map[i].base;
        uint64_t length = e820_map[i].length;
        uint64_t top    = base + length;

        // Check if the entry overlaps non-usable entries
        for (size_t j = 0; j < e820_entries; j++) {
            if (e820_map[j].type == 1)
                continue;

            size_t res_base   = e820_map[j].base;
            size_t res_length = e820_map[j].length;
            size_t res_top    = res_base + res_length;

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
        }

        if (align_entry_with_base(&base, &length))
            continue;

        memmap[memmap_entries].type   = 1;
        memmap[memmap_entries].base   = base;
        memmap[memmap_entries].length = length;

        memmap_entries++;
    }
}

void memmap_alloc_range(uint64_t base, uint64_t length) {
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

            target->type   = 10;
            target->base   = base;
            target->length = length;

            return;
        }
    }

    panic("Out of memory");
}
