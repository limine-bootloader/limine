#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/pmm.h>
#include <lib/rand.h>
#include <lib/print.h>

static bool full_overlap_check(uint64_t base1, uint64_t top1,
                               uint64_t base2, uint64_t top2) {
    return ((base1 >= base2 && base1 <  top2)
         && (top1  >  base2 && top1  <= top2));
}

bool check_usable_memory(uint64_t base, uint64_t top) {
    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE) {
            continue;
        }

        if (full_overlap_check(base, top, memmap[i].base, memmap[i].base + memmap[i].length)) {
            return true;
        }
    }

    return false;
}

void pmm_randomise_memory(void) {
    print("pmm: Randomising memory contents...");

    for (size_t i = 0; i < memmap_entries; i++) {
        if (memmap[i].type != MEMMAP_USABLE)
            continue;

#if defined (BIOS)
        // We're not going to randomise memory above 4GiB from protected mode,
        // are we?
        if (memmap[i].base >= 0x100000000) {
            continue;
        }
#endif

        uint8_t *ptr = (void *)(uintptr_t)memmap[i].base;
        size_t len = memmap[i].length;

        for (size_t j = 0;;) {
            uint32_t random = rand32();
            uint8_t *rnd_data = (void *)&random;
            if (j >= len)
                break;
            ptr[j++] = rnd_data[0];
            if (j >= len)
                break;
            ptr[j++] = rnd_data[1];
            if (j >= len)
                break;
            ptr[j++] = rnd_data[2];
            if (j >= len)
                break;
            ptr[j++] = rnd_data[3];
        }
    }

    print("\n");
}
