#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/libc.h>
#include <lib/acpi.h>
#include <sys/cpu.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <sys/smp.h>
#include <sys/lapic.h>
#include <sys/gdt.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

struct madt {
    struct sdt header;
    uint32_t local_controller_addr;
    uint32_t flags;
    char     madt_entries_begin[];
} __attribute__((packed));

struct madt_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_lapic {
    struct madt_header header;
    uint8_t  acpi_processor_uid;
    uint8_t  lapic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_x2apic {
    struct madt_header header;
    uint8_t  reserved[2];
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t acpi_processor_uid;
} __attribute__((packed));

extern symbol smp_trampoline_start;
extern size_t smp_trampoline_size;

struct madt_gicc {
    struct madt_header header;
    uint8_t  reserved1[2];
    uint32_t iface_no;
    uint32_t acpi_uid;
    uint32_t flags;
    uint32_t parking_ver;
    uint32_t perf_gsiv;
    uint64_t parking_addr;
    uint64_t gicc_base_addr;
    uint64_t gicv_base_addr;
    uint64_t gich_base_addr;
    uint32_t vgic_maint_gsiv;
    uint64_t gicr_base_addr;
    uint64_t mpidr;
    uint8_t  power_eff_class;
    uint8_t  reserved2;
    uint16_t spe_overflow_gsiv;
} __attribute__((packed));

#if defined (__x86_64__) || defined (__i386__)

struct trampoline_passed_info {
    uint8_t  smp_tpl_booted_flag;
    uint8_t  smp_tpl_target_mode;
    uint32_t smp_tpl_pagemap;
    uint32_t smp_tpl_info_struct;
    struct gdtr smp_tpl_gdt;
    uint64_t smp_tpl_hhdm;
} __attribute__((packed));

static bool smp_start_ap(uint32_t lapic_id, struct gdtr *gdtr,
                         struct smp_information *info_struct,
                         bool longmode, bool lv5, uint32_t pagemap,
                         bool x2apic, bool nx, uint64_t hhdm, bool wp) {
    // Prepare the trampoline
    static void *trampoline = NULL;
    if (trampoline == NULL) {
        trampoline = conv_mem_alloc(smp_trampoline_size);

        memcpy(trampoline, smp_trampoline_start, smp_trampoline_size);
    }

    static struct trampoline_passed_info *passed_info = NULL;
    if (passed_info == NULL) {
        passed_info = (void *)(((uintptr_t)trampoline + smp_trampoline_size)
                               - sizeof(struct trampoline_passed_info));
    }

    passed_info->smp_tpl_info_struct = (uint32_t)(uintptr_t)info_struct;
    passed_info->smp_tpl_booted_flag = 0;
    passed_info->smp_tpl_pagemap     = pagemap;
    passed_info->smp_tpl_target_mode = ((uint32_t)x2apic << 2)
                        | ((uint32_t)lv5 << 1)
                        | ((uint32_t)nx << 3)
                        | ((uint32_t)wp << 4)
                        | (uint32_t)longmode;
    passed_info->smp_tpl_gdt         = *gdtr;
    passed_info->smp_tpl_hhdm = hhdm;

    asm volatile ("" ::: "memory");

    // Send the INIT IPI
    if (x2apic) {
        x2apic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) | 0x4500);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, 0x4500);
    }
    delay(10000000);

    // Send the Startup IPI
    if (x2apic) {
        x2apic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) |
                                     ((size_t)trampoline / 4096) | 0x4600);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, ((size_t)trampoline / 4096) | 0x4600);
    }

    for (int i = 0; i < 100; i++) {
        if (locked_read(&passed_info->smp_tpl_booted_flag) == 1) {
            return true;
        }
        delay(10000000);
    }

    return false;
}

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
    if (!lapic_check())
        return NULL;

    // Search for MADT table
    struct madt *madt = acpi_get_table("APIC", 0);

    if (madt == NULL)
        return NULL;

    struct gdtr gdtr = gdt;

    uint32_t eax, ebx, ecx, edx;

    if (!cpuid(1, 0, &eax, &ebx, &ecx, &edx))
        return NULL;

    uint8_t bsp_lapic_id = ebx >> 24;

    x2apic = x2apic && x2apic_enable();

    uint32_t bsp_x2apic_id = 0;
    if (x2apic) {
        // The Intel manual recommends checking if leaf 0x1f exists first, and
        // using that in place of 0xb if that's the case
        if (!cpuid(0x1f, 0, &eax, &ebx, &ecx, &edx))
            if (!cpuid(0xb, 0, &eax, &ebx, &ecx, &edx))
                return NULL;

        bsp_x2apic_id = edx;

        *_bsp_lapic_id = bsp_x2apic_id;
    } else {
        *_bsp_lapic_id = bsp_lapic_id;
    }

    *cpu_count = 0;

    // Count the MAX of startable APs and allocate accordingly
    size_t max_cpus = 0;

    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 0: {
                // Processor local xAPIC
                struct madt_lapic *lapic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if ((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1))
                    max_cpus++;

                continue;
            }
            case 9: {
                // Processor local x2APIC
                if (!x2apic)
                    continue;

                struct madt_x2apic *x2lapic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if ((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1))
                    max_cpus++;

                continue;
            }
        }
    }

    *header_ptr = ext_mem_alloc(
                  header_hack_size + max_cpus * sizeof(struct smp_information));
    struct smp_information *ret = *header_ptr + header_hack_size;
    *cpu_count = 0;

    // Try to start all APs
    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 0: {
                // Processor local xAPIC
                struct madt_lapic *lapic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (!((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1)))
                    continue;

                struct smp_information *info_struct = &ret[*cpu_count];

                info_struct->acpi_processor_uid = lapic->acpi_processor_uid;
                info_struct->lapic_id           = lapic->lapic_id;

                // Do not try to restart the BSP
                if (lapic->lapic_id == bsp_lapic_id) {
                    (*cpu_count)++;
                    continue;
                }

                printv("smp: [xAPIC] Found candidate AP for bring-up. LAPIC ID: %u\n", lapic->lapic_id);

                // Try to start the AP
                if (!smp_start_ap(lapic->lapic_id, &gdtr, info_struct,
                                  longmode, lv5, (uintptr_t)pagemap.top_level,
                                  x2apic, nx, hhdm, wp)) {
                    print("smp: FAILED to bring-up AP\n");
                    continue;
                }

                printv("smp: Successfully brought up AP\n");

                (*cpu_count)++;
                continue;
            }
            case 9: {
                // Processor local x2APIC
                if (!x2apic)
                    continue;

                struct madt_x2apic *x2lapic = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (!((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1)))
                    continue;

                struct smp_information *info_struct = &ret[*cpu_count];

                info_struct->acpi_processor_uid = x2lapic->acpi_processor_uid;
                info_struct->lapic_id           = x2lapic->x2apic_id;

                // Do not try to restart the BSP
                if (x2lapic->x2apic_id == bsp_x2apic_id) {
                    (*cpu_count)++;
                    continue;
                }

                printv("smp: [x2APIC] Found candidate AP for bring-up. LAPIC ID: %u\n", x2lapic->x2apic_id);

                // Try to start the AP
                if (!smp_start_ap(x2lapic->x2apic_id, &gdtr, info_struct,
                                  longmode, lv5, (uintptr_t)pagemap.top_level,
                                  true, nx, hhdm, wp)) {
                    print("smp: FAILED to bring-up AP\n");
                    continue;
                }

                printv("smp: Successfully brought up AP\n");

                (*cpu_count)++;
                continue;
            }
        }
    }

    return ret;
}

#elif defined (__aarch64__)

struct trampoline_passed_info {
    uint64_t smp_tpl_booted_flag;

    uint64_t smp_tpl_ttbr0;
    uint64_t smp_tpl_ttbr1;

    uint64_t smp_tpl_mair;
    uint64_t smp_tpl_tcr;
    uint64_t smp_tpl_sctlr;

    uint64_t smp_tpl_info_struct;
};

enum {
    BOOT_WITH_SPIN_TBL,
    BOOT_WITH_PSCI_SMC,
    BOOT_WITH_PSCI_HVC,
    BOOT_WITH_ACPI_PARK
};

static uint32_t psci_cpu_on = 0xC4000003;

static bool try_start_ap(int boot_method, uint64_t method_ptr,
                         struct smp_information *info_struct,
                         uint64_t ttbr0, uint64_t ttbr1, uint64_t mair,
                         uint64_t tcr, uint64_t sctlr) {
    // Prepare the trampoline
    static void *trampoline = NULL;
    if (trampoline == NULL) {
        trampoline = ext_mem_alloc(0x1000);

        memcpy(trampoline, smp_trampoline_start, smp_trampoline_size);
    }

    static struct trampoline_passed_info *passed_info = NULL;
    if (passed_info == NULL) {
        passed_info = (void *)(((uintptr_t)trampoline + 0x1000)
                               - sizeof(struct trampoline_passed_info));
    }

    passed_info->smp_tpl_info_struct = (uint64_t)(uintptr_t)info_struct;
    passed_info->smp_tpl_booted_flag = 0;
    passed_info->smp_tpl_ttbr0       = ttbr0;
    passed_info->smp_tpl_ttbr1       = ttbr1;
    passed_info->smp_tpl_mair        = mair;
    passed_info->smp_tpl_tcr         = tcr;
    passed_info->smp_tpl_sctlr       = sctlr;

    // Cache coherency between the I-Cache and D-Cache is not guaranteed by the 
    // architecture and as such we must perform I-Cache invalidation.
    // Additionally, the newly-booted AP may have caches disabled which implies
    // it possibly does not see our cache contents either.

    clean_inval_dcache_poc((uintptr_t)trampoline, (uintptr_t)trampoline + 0x1000);
    inval_icache_pou((uintptr_t)trampoline, (uintptr_t)trampoline + 0x1000);

    asm volatile ("" ::: "memory");

    switch (boot_method) {
        case BOOT_WITH_SPIN_TBL:
            *(volatile uint64_t *)method_ptr = (uint64_t)(uintptr_t)trampoline;
            clean_inval_dcache_poc(method_ptr, method_ptr + 8);
            asm ("sev");
            break;

        case BOOT_WITH_PSCI_SMC:
        case BOOT_WITH_PSCI_HVC: {
            register int32_t result asm("w0");
            register uint32_t cmd asm("w0") = psci_cpu_on;
            register uint64_t cpu asm("x1") = info_struct->mpidr;
            register uint64_t addr asm("x2") = (uint64_t)(uintptr_t)trampoline;
            register uint64_t ctx asm("x3") = 0;

            if (boot_method == BOOT_WITH_PSCI_SMC)
                asm volatile ("smc #0" : "=r"(result) : "r"(cmd), "r"(cpu), "r"(addr), "r"(ctx));
            else
                asm volatile ("hvc #0" : "=r"(result) : "r"(cmd), "r"(cpu), "r"(addr), "r"(ctx));

            switch (result) {
                case 0: // Success
                    break;
                case -2:
                    printv("smp: PSCI says CPU_ON was given invalid arguments\n");
                    return false;
                case -4:
                    printv("smp: PSCI says AP is already on\n");
                    return false;
                case -5:
                    printv("smp: PSCI says CPU_ON is already pending for this AP\n");
                    return false;
                case -6:
                    printv("smp: PSCI reports internal failure\n");
                    return false;
                case -9:
                    printv("smp: PSCI says CPU_ON was given an invalid address\n");
                    return false;
                default:
                    printv("smp: PSCI reports an unexpected error (%d)\n", result);
                    return false;
            }

            break;
        }

        case BOOT_WITH_ACPI_PARK:
            panic(false, "ACPI parking protocol is unsupported, please report this!");
            break;

        default:
            panic(false, "Invalid boot method specified");
    }

    for (int i = 0; i < 1000000; i++) {
        // We do not need cache invalidation here as by the time the AP gets to
        // set this flag, it has enabled it's caches

        if (locked_read(&passed_info->smp_tpl_booted_flag) == 1) {
            return true;
        }
        //delay(10000000);
    }

    return false;
}

static struct smp_information *try_acpi_smp(size_t    header_hack_size,
                                            void    **header_ptr,
                                            size_t   *cpu_count,
                                            uint64_t *_bsp_mpidr,
                                            pagemap_t pagemap,
                                            uint64_t  mair,
                                            uint64_t  tcr,
                                            uint64_t  sctlr) {
    int boot_method = BOOT_WITH_ACPI_PARK;

    // Search for FADT table
    uint8_t *fadt = acpi_get_table("FACP", 0);

    if (fadt == NULL)
        return NULL;

    // Read the single field from the FADT without defining a struct for the whole table
    uint16_t arm_boot_args;
    memcpy(&arm_boot_args, fadt + 129, 2);

    if (arm_boot_args & 1) // PSCI compliant?
        boot_method = arm_boot_args & 2 ? BOOT_WITH_PSCI_HVC : BOOT_WITH_PSCI_SMC;

    // Search for MADT table
    struct madt *madt = acpi_get_table("APIC", 0);

    if (madt == NULL)
        return NULL;

    uint64_t bsp_mpidr;
    asm volatile ("mrs %0, mpidr_el1" : "=r"(bsp_mpidr));

    // This bit is Res1 in the system reg, but not included in the MPIDR from MADT 
    bsp_mpidr &= ~((uint64_t)1 << 31);

    *_bsp_mpidr = bsp_mpidr;

    printv("smp: BSP MPIDR is %X\n", bsp_mpidr);

    *cpu_count = 0;

    // Count the MAX of startable APs and allocate accordingly
    size_t max_cpus = 0;

    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 11: {
                // GIC CPU Interface
                struct madt_gicc *gicc = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (gicc->flags & 1)
                    max_cpus++;

                continue;
            }
        }
    }

    *header_ptr = ext_mem_alloc(
                  header_hack_size + max_cpus * sizeof(struct smp_information));
    struct smp_information *ret = *header_ptr + header_hack_size;
    *cpu_count = 0;

    // Try to start all APs
    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
      (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
      madt_ptr += *(madt_ptr + 1)) {
        switch (*madt_ptr) {
            case 11: {
                // GIC CPU Interface
                struct madt_gicc *gicc = (void *)madt_ptr;

                // Check if we can actually try to start the AP
                if (!(gicc->flags & 1))
                    continue;

                struct smp_information *info_struct = &ret[*cpu_count];

                info_struct->acpi_processor_uid = gicc->acpi_uid;
                info_struct->gic_iface_no       = gicc->iface_no;
                info_struct->mpidr              = gicc->mpidr;

                // Do not try to restart the BSP
                if (gicc->mpidr == bsp_mpidr) {
                    (*cpu_count)++;
                    continue;
                }

                printv("smp: Found candidate AP for bring-up. Interface no.: %x, MPIDR: %X\n", gicc->iface_no, gicc->mpidr);

                // Try to start the AP
                if (!try_start_ap(boot_method, gicc->parking_addr, info_struct,
                                  (uint64_t)(uintptr_t)pagemap.top_level[0],
                                  (uint64_t)(uintptr_t)pagemap.top_level[1],
                                  mair, tcr, sctlr)) {
                    print("smp: FAILED to bring-up AP\n");
                    continue;
                }

                printv("smp: Successfully brought up AP\n");

                (*cpu_count)++;
                continue;
            }
        }
    }

    return ret;
}

struct smp_information *init_smp(size_t    header_hack_size,
                                 void    **header_ptr,
                                 size_t   *cpu_count,
                                 uint64_t *bsp_mpidr,
                                 pagemap_t pagemap,
                                 uint64_t  mair,
                                 uint64_t  tcr,
                                 uint64_t  sctlr) {
    struct smp_information *info = NULL;

    //if (dtb_is_present() && (info = try_dtb_smp(header_hack_size, header_ptr, cpu_count,
    //                _bsp_iface_no, pagemap, mair, tcr, sctlr)))
    //    return info;

    // No RSDP means no ACPI
    if (acpi_get_rsdp() && (info = try_acpi_smp(header_hack_size, header_ptr, cpu_count,
                    bsp_mpidr, pagemap, mair, tcr, sctlr)))
        return info;

    printv("Failed to figure out how to start APs.");

    return NULL;
}

#else
#error Unknown architecture
#endif
