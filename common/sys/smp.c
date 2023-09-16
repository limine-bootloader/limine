#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/libc.h>
#include <lib/acpi.h>
#include <sys/cpu.h>
#include <lib/misc.h>
#include <lib/print.h>
#include <sys/smp.h>
#include <sys/lapic.h>
#include <sys/gdt.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#define LIMINE_NO_POINTERS
#include <limine.h>
#if defined (__riscv64)
#include <sys/sbi.h>
#endif

extern symbol smp_trampoline_start;
extern size_t smp_trampoline_size;

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
                         struct limine_smp_info *info_struct,
                         bool longmode, int paging_mode, uint32_t pagemap,
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
                                     | ((uint32_t)paging_mode << 1)
                                     | ((uint32_t)nx << 3)
                                     | ((uint32_t)wp << 4)
                                     | ((uint32_t)longmode << 0);
    passed_info->smp_tpl_gdt = *gdtr;
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

struct limine_smp_info *init_smp(size_t   *cpu_count,
                                 uint32_t *_bsp_lapic_id,
                                 bool      longmode,
                                 int       paging_mode,
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

    struct limine_smp_info *ret = ext_mem_alloc(max_cpus * sizeof(struct limine_smp_info));
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

                struct limine_smp_info *info_struct = &ret[*cpu_count];

                info_struct->processor_id = lapic->acpi_processor_uid;
                info_struct->lapic_id = lapic->lapic_id;

                // Do not try to restart the BSP
                if (lapic->lapic_id == bsp_lapic_id) {
                    (*cpu_count)++;
                    continue;
                }

                printv("smp: [xAPIC] Found candidate AP for bring-up. LAPIC ID: %u\n", lapic->lapic_id);

                // Try to start the AP
                if (!smp_start_ap(lapic->lapic_id, &gdtr, info_struct,
                                  longmode, paging_mode, (uintptr_t)pagemap.top_level,
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

                struct limine_smp_info *info_struct = &ret[*cpu_count];

                info_struct->processor_id = x2lapic->acpi_processor_uid;
                info_struct->lapic_id = x2lapic->x2apic_id;

                // Do not try to restart the BSP
                if (x2lapic->x2apic_id == bsp_x2apic_id) {
                    (*cpu_count)++;
                    continue;
                }

                printv("smp: [x2APIC] Found candidate AP for bring-up. LAPIC ID: %u\n", x2lapic->x2apic_id);

                // Try to start the AP
                if (!smp_start_ap(x2lapic->x2apic_id, &gdtr, info_struct,
                                  longmode, paging_mode, (uintptr_t)pagemap.top_level,
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
                         struct limine_smp_info *info_struct,
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

    clean_dcache_poc((uintptr_t)trampoline, (uintptr_t)trampoline + 0x1000);
    inval_icache_pou((uintptr_t)trampoline, (uintptr_t)trampoline + 0x1000);

    asm volatile ("" ::: "memory");

    switch (boot_method) {
        case BOOT_WITH_SPIN_TBL:
            *(volatile uint64_t *)method_ptr = (uint64_t)(uintptr_t)trampoline;
            clean_dcache_poc(method_ptr, method_ptr + 8);
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

static struct limine_smp_info *try_acpi_smp(size_t   *cpu_count,
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

    struct limine_smp_info *ret = ext_mem_alloc(max_cpus * sizeof(struct limine_smp_info));
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

                struct limine_smp_info *info_struct = &ret[*cpu_count];

                info_struct->processor_id = gicc->acpi_uid;
                info_struct->gic_iface_no = gicc->iface_no;
                info_struct->mpidr = gicc->mpidr;

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

struct limine_smp_info *init_smp(size_t   *cpu_count,
                                 uint64_t *bsp_mpidr,
                                 pagemap_t pagemap,
                                 uint64_t  mair,
                                 uint64_t  tcr,
                                 uint64_t  sctlr) {
    struct limine_smp_info *info = NULL;

    //if (dtb_is_present() && (info = try_dtb_smp(cpu_count,
    //                _bsp_iface_no, pagemap, mair, tcr, sctlr)))
    //    return info;

    // No RSDP means no ACPI
    if (acpi_get_rsdp() && (info = try_acpi_smp(cpu_count,
                    bsp_mpidr, pagemap, mair, tcr, sctlr)))
        return info;

    printv("Failed to figure out how to start APs.");

    return NULL;
}

#elif defined (__riscv64)

struct trampoline_passed_info {
    uint64_t smp_tpl_booted_flag;
    uint64_t smp_tpl_satp;
    uint64_t smp_tpl_info_struct;
};

static bool smp_start_ap(size_t hartid, size_t satp, struct limine_smp_info *info_struct) {
    static struct trampoline_passed_info passed_info;

    passed_info.smp_tpl_booted_flag = 0;
    passed_info.smp_tpl_satp        = satp;
    passed_info.smp_tpl_info_struct = (uint64_t)info_struct;

    asm volatile ("" ::: "memory");

    struct sbiret ret = sbi_hart_start(hartid, (size_t)smp_trampoline_start, (size_t)&passed_info);
    if (ret.error != SBI_SUCCESS)
        return false;

    for (int i = 0; i < 1000000; i++) {
        if (locked_read(&passed_info.smp_tpl_booted_flag) == 1)
            return true;
    }

    return false;
}

struct limine_smp_info *init_smp(size_t *cpu_count, pagemap_t pagemap) {
    size_t num_cpus = 0;
    for (struct riscv_hart *hart = hart_list; hart != NULL; hart = hart->next) {
        if (!(hart->flags & RISCV_HART_COPROC)) {
            num_cpus += 1;
        }
    }

    struct limine_smp_info *ret = ext_mem_alloc(num_cpus * sizeof(struct limine_smp_info));
    if (ret == NULL) {
        panic(false, "out of memory");
    }

    *cpu_count = 0;
    for (struct riscv_hart *hart = hart_list; hart != NULL; hart = hart->next) {
        if (hart->flags & RISCV_HART_COPROC) {
            continue;
        }
        struct limine_smp_info *info_struct = &ret[*cpu_count];

        info_struct->hartid = hart->hartid;
        info_struct->processor_id = hart->acpi_uid;

        // Don't try to start the BSP.
        if (hart->hartid == bsp_hartid) {
            *cpu_count += 1;
            continue;
        }

        printv("smp: Found candidate AP for bring-up. Hart ID: %u\n", hart->hartid);

        // Try to start the AP.
        size_t satp = make_satp(pagemap.paging_mode, pagemap.top_level);
        if (!smp_start_ap(hart->hartid, satp, info_struct)) {
            print("smp: FAILED to bring-up AP\n");
            continue;
        }

        (*cpu_count)++;
        continue;
    }

    return ret;
}

#else
#error Unknown architecture
#endif
