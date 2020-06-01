#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <lib/rand.h>

// TODO: Find where this mersenne twister implementation is inspired from
//       and properly credit the original author(s).

#define rdrand(type) ({ \
    type ret; \
    asm volatile ( \
        "1: " \
        "rdrand %0;" \
        "jnc 1b;" \
        : "=r" (ret) \
    ); \
    ret; \
})

#define rdseed(type) ({ \
    type ret; \
    asm volatile ( \
        "1: " \
        "rdrand %0;" \
        "jnc 1b;" \
        : "=r" (ret) \
    ); \
    ret; \
})

#define rdtsc(type) ({ \
    type ret; \
    asm volatile ( \
        "rdtsc;" \
        : "=A" (ret) \
    ); \
    ret; \
})

static bool rand_initialised = false;

static void init_rand(void) {
    uint32_t seed = ((uint32_t)0xc597060c * rdtsc(uint32_t))
                  * ((uint32_t)0xce86d624)
                  ^ ((uint32_t)0xee0da130 * rdtsc(uint32_t));

    uint32_t eax, ebx, ecx, edx;

    // Check for rdseed
    if (!cpuid(0x07, 0, &eax, &ebx, &ecx, &edx) && (ebx & (1 << 18))) {
        seed *= (seed ^ rdseed(uint32_t));
    } else if (!cpuid(0x01, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 30))) {
        seed *= (seed ^ rdrand(uint32_t));
    }

    srand(seed);

    rand_initialised = true;
}

#define n ((int)624)
#define m ((int)397)
#define matrix_a ((uint32_t)0x9908b0df)
#define msb ((uint32_t)0x80000000)
#define lsbs ((uint32_t)0x7fffffff)

static uint32_t status[n];
static int ctr;

void srand(uint32_t s) {
    status[0] = s;
    for (ctr = 1; ctr < n; ctr++)
        status[ctr] = (1812433253 * (status[ctr - 1] ^ (status[ctr - 1] >> 30)) + ctr);
}

uint32_t rand32(void) {
    if (!rand_initialised)
        init_rand();

    const uint32_t mag01[2] = {0, matrix_a};

    if (ctr >= n) {
        for (int kk = 0; kk < n - m; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + m] ^ (y >> 1) ^ mag01[y & 1];
        }

        for (int kk = n - m; kk < n - 1; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + (m - n)] ^ (y >> 1) ^ mag01[y & 1];
        }

        uint32_t y = (status[n - 1] & msb) | (status[0] & lsbs);
        status[n - 1] = status[m - 1] ^ (y >> 1) ^ mag01[y & 1];

        ctr = 0;
    }

    uint32_t res = status[ctr++];

    res ^= (res >> 11);
    res ^= (res << 7) & 0x9d2c5680;
    res ^= (res << 15) & 0xefc60000;
    res ^= (res >> 18);

    return res;
}

uint64_t rand64(void) {
    return (((uint64_t)rand32()) << 32) | (uint64_t)rand32();
}
