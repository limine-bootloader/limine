#include <stdint.h>
#include <stddef.h>
#include <mm/mtrr.h>
#include <mm/pmm.h>
#include <sys/cpu.h>
#include <lib/print.h>
#include <lib/blib.h>

static bool mtrr_supported(void) {
    uint32_t eax, ebx, ecx, edx;

    if (!cpuid(1, 0, &eax, &ebx, &ecx, &edx))
        return false;

    return !!(edx & (1 << 12));
}

static bool is_block_in_mtrr_range(struct mtrr *mtrr, uint64_t block_base, uint64_t block_size) {
    // False if the MTRR is not valid
    if (!(mtrr->mask & (1 << 11)))
        return false;

    uint64_t base = mtrr->base & ~((uint64_t)0xfff);
    uint64_t mask = mtrr->mask & ~((uint64_t)0xfff);

    for (uint64_t i = block_base; i < block_base + block_size; i += 4096) {
        if ((i & mask) == (base & mask))
            return true;
    }

    return false;
}

bool mtrr_set_range(uint64_t base, uint64_t size, uint8_t memory_type) {
    if (!mtrr_supported())
        return false;

    uint32_t eax, ebx, ecx, edx;

    if (!cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx))
        return false;

    uint8_t maxphysaddr = eax & 0xff;
    print("mtrr: Max phys addr: %u\n", maxphysaddr);

    base = ALIGN_DOWN(base, 0x1000);

    // Size must be aligned on a power of 2 (this is a slow method but this is
    // not time sensitive)
    for (uint64_t aligned_size = 1; ; aligned_size *= 2) {
        if (aligned_size >= size) {
            size = aligned_size;
            break;
        }
    }

    size = ALIGN_UP(size, 0x1000);
    uint64_t mask = (((uint64_t)1 << maxphysaddr) - 1) & ~(size - 1);

    print("mtrr: Base: %X Mask: %X\n", base, mask);

    uint64_t ia32_mtrrcap = rdmsr(0xfe);

    if (ia32_mtrrcap & (1 << 10)) {
        print("mtrr: Write-combining supported\n");
    } else {
        if (memory_type == MTRR_MEMORY_TYPE_WC)
            return false;
    }

    uint8_t var_reg_count = ia32_mtrrcap & 0xff;

    // Check if we're not overlapping any other MTRR range
    for (uint8_t i = 0; i < var_reg_count; i++) {
        struct mtrr mtrr;
        mtrr.base = rdmsr(0x200 + i * 2);
        mtrr.mask = rdmsr(0x200 + i * 2 + 1);

        if (is_block_in_mtrr_range(&mtrr, base, size))
            return false;
    }

    print("mtrr: Block does not overlap other ranges, good to go\n");

    // Find usable MTRR slot
    for (uint8_t i = 0; i < var_reg_count; i++) {
        struct mtrr mtrr;
        mtrr.base = rdmsr(0x200 + i * 2);
        mtrr.mask = rdmsr(0x200 + i * 2 + 1);

        if (mtrr.mask & (1 << 11))
            continue;

        // Found
        wrmsr(0x200 + i * 2,     base | memory_type);
        wrmsr(0x200 + i * 2 + 1, mask | (1 << 11));

        print("mtrr: Set range in variable MTRR number %u\n", i);
        return true;
    }

    return false;
}

struct mtrr *saved_mtrr = NULL;

void mtrr_save(void) {
    if (!mtrr_supported())
        return;

    uint64_t ia32_mtrrcap = rdmsr(0xfe);

    uint8_t var_reg_count = ia32_mtrrcap & 0xff;

    if (!saved_mtrr)
        saved_mtrr = ext_mem_alloc(var_reg_count * sizeof(struct mtrr));

    for (uint8_t i = 0; i < var_reg_count; i++) {
        saved_mtrr[i].base = rdmsr(0x200 + i * 2);
        saved_mtrr[i].mask = rdmsr(0x200 + i * 2 + 1);
    }
}

void mtrr_restore(void) {
    if (!mtrr_supported())
        return;

    uint64_t ia32_mtrrcap = rdmsr(0xfe);

    uint8_t var_reg_count = ia32_mtrrcap & 0xff;

    for (uint8_t i = 0; i < var_reg_count; i++) {
        wrmsr(0x200 + i * 2,     saved_mtrr[i].base);
        wrmsr(0x200 + i * 2 + 1, saved_mtrr[i].mask);
    }
}
