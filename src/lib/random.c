#include <lib/blib.h>

int rdrand_available = 0;

static void check_rdrand(void) {
    uint32_t eax, ebx, ecx, edx;
    int ret = cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (ret)
        return;

    if (ecx & (1 << 30))
        rdrand_available = 1;
}

static void init_simple_rand(void) {
    // TODO: Some fallback randomness init
}

void init_random(void) {
    check_rdrand();
    if (!rdrand_available) {
        init_simple_rand();
    }
}

static uint32_t rdrand(void) {
    uint32_t val;

    asm (
        "1:rdrand %0\n\t"
        "jnc 1b\n\t"
        :"=r"(val)
    );

    return val;
}

static uint32_t simple_rand(void) {
    // TODO: Some fallback randomness
    return 0xFEEDFACE;
}

uint32_t get_random(void) {
    if (rdrand_available)
        return rdrand();
    else
        return simple_rand();
}
