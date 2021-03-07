#include <stdint.h>
#include <stddef.h>
#include <mm/mtrr.h>
#include <sys/cpu.h>
#include <lib/blib.h>

static bool mtrr_supported(void) {
    uint32_t eax, ebx, ecx, edx;

    if (!cpuid(1, 0, &eax, &ebx, &ecx, &edx))
        return false;

    return !!(edx & (1 << 12));
}

void mtrr_restore_32(struct mtrr *saved_mtrr) {
    if (!mtrr_supported())
        return;

    uint64_t ia32_mtrrcap = rdmsr(0xfe);

    uint8_t var_reg_count = ia32_mtrrcap & 0xff;

    for (uint8_t i = 0; i < var_reg_count; i++) {
        wrmsr(0x200 + i * 2,     saved_mtrr[i].base);
        wrmsr(0x200 + i * 2 + 1, saved_mtrr[i].mask);
    }
}
