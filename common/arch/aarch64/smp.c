#include <lib/print.h>
#include <arch/aarch64/smp.h>
#include <arch/aarch64/dtb.h>

struct smp_information *init_smp(size_t    header_hack_size,
                                 void    **header_ptr,
                                 size_t   *cpu_count,
                                 uint32_t *_bsp_lapic_id,
                                 bool      longmode,
                                 bool      lv5,
                                 pagemap_t pagemap,
                                 bool      x2apic,
                                 bool      nx,
                                 uint64_t  hhdm,
                                 bool      wp) {
    (void)header_hack_size;
    (void)header_ptr;
    (void)cpu_count;
    (void)_bsp_lapic_id;
    (void)longmode;
    (void)lv5;
    (void)pagemap;
    (void)x2apic;
    (void)nx;
    (void)hhdm;
    (void)wp;

    struct dtb dtb;
    if (!dtb_init(&dtb)) return NULL;

    return NULL;
}
