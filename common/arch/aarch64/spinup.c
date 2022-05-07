#include <lib/blib.h>
#include <mm/vmm.h>

extern symbol vbar;
#define vbar_entry(what) ".align 7\nb .\n.asciz \"vbar_el1 entry: " #what "\"\n"
asm("\
.align 11\n\
vbar:"
vbar_entry(curr_el_sp0_sync)
vbar_entry(curr_el_sp0_irq)
vbar_entry(curr_el_sp0_fiq)
vbar_entry(curr_el_sp0_serror)
vbar_entry(curr_el_spx_sync)
vbar_entry(curr_el_spx_irq)
vbar_entry(curr_el_spx_fiq)
vbar_entry(curr_el_spx_serror)
vbar_entry(lower_el_sp0_sync)
vbar_entry(lower_el_sp0_irq)
vbar_entry(lower_el_sp0_fiq)
vbar_entry(lower_el_sp0_serror)
vbar_entry(lower_el_spx_sync)
vbar_entry(lower_el_spx_irq)
vbar_entry(lower_el_spx_fiq)
vbar_entry(lower_el_spx_serror)
);

noreturn void common_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stivale_struct, uint64_t stack,
                 bool enable_nx, bool wp) {
    uint64_t sctlr, aa64mmfr0, currentel;
    asm (
        "MRS %[sctlr], SCTLR_EL1\n"
        "MRS %[aa64mmfr0], ID_AA64MMFR0_EL1\n"
        "MRS %[currentel], CurrentEL\n"
        : [sctlr] "=r"(sctlr),
        [aa64mmfr0] "=r"(aa64mmfr0),
        [currentel] "=r"(currentel)
    );
    
    sctlr |= 1; // sctlr_el1.M = 1
    sctlr &= ~2; // sctlr_el1.A = 0

    uint64_t paging_granule_br0;
    uint64_t paging_granule_br1;
    uint64_t region_size_offset;

    switch (get_page_size()) {
        case 0x1000: {
            paging_granule_br0 = 0b00;
            paging_granule_br1 = 0b10;
            region_size_offset = 16;
            break;
        }
        case 0x4000: {
            paging_granule_br0 = 0b10;
            paging_granule_br1 = 0b01;
            region_size_offset = 8;
            break;
        }
        case 0x10000: {
            paging_granule_br0 = 0b01;
            paging_granule_br1 = 0b11;
            region_size_offset = 0;
            break;
        }
        default: __builtin_unreachable();
    }

    uint64_t tcr = 0
        | (region_size_offset << 0) // T0SZ
        | (region_size_offset << 16) // T1SZ
        | (1 << 8) // TTBR0 Inner WB RW-Allocate
        | (1 << 10) // TTBR0 Outer WB RW-Allocate
        | (1 << 24) // TTBR1 Inner WB RW-Allocate
        | (1 << 26) // TTBR1 Outer WB RW-Allocate
        | (2 << 12) // TTBR0 Inner shareable
        | (2 << 28) // TTBR1 Inner shareable
        | (aa64mmfr0 << 32) // intermediate address size
        | (paging_granule_br0 << 14) // TTBR0 granule
        | (paging_granule_br1 << 30) // TTBR1 granule
        | (1ULL << 56) // Fault on TTBR1 access from EL0
        | (0ULL << 55) // Don't fault on TTBR0 access from EL0
    ;

    uint64_t mair = 0
        | (0b11111111 << 0) // Normal, Write-back RW-Allocate non-transient
        | (0b00000000 << 8) // Device, nGnRnE
    ;

    asm volatile (
        "msr TTBR0_EL1, %[ttbr0]\n"
        "msr TTBR1_EL1, %[ttbr1]\n"
        "msr MAIR_EL1, %[mair]\n"
        "msr TCR_EL1, %[tcr]\n"
        "msr SCTLR_EL1, %[sctlr]\n"
        "msr VBAR_EL1, %[vbar]\n"
        "dsb sy\n"
        "isb sy\n"
        :
        : [ttbr0] "r" (pagemap->ttbr0),
          [ttbr1] "r" (pagemap->ttbr1),
          [sctlr] "r" (sctlr),
          [tcr] "r" (tcr),
          [mair] "r" (mair),
          [vbar] "r" (vbar)
        : "memory"
    );
    

    if (currentel == /* el2 */ 8) {
        // drop down to el1 (borrowed from sabaton)
        asm volatile (
            // aarch64 in EL1
            "  orr x1, xzr, #(1 << 31)\n"
            "  orr x1, x1,  #(1 << 1)\n"
            "  msr hcr_el2, x1\n"

            // Counters in EL1
            "  mrs x1, cnthctl_el2\n"
            "  orr x1, x1, #3\n"
            "  msr cnthctl_el2, x1\n"
            "  msr cntvoff_el2, xzr\n"

            // FP/SIMD in EL1
            "  mov x1, #0x33ff\n"
            "  msr cptr_el2, x1\n"
            "  msr hstr_el2, xzr\n"
            "  mov x1, #0x300000\n"
            "  msr cpacr_el1, x1\n"

            // Get the fuck out of EL2 into EL1
            "  msr elr_el2, lr\n"
            "  mov x1, #0x3c5\n"
            "  msr spsr_el2, x1\n"

            
            "  mov x0, %[info]\n"
            "  msr SPSel, XZR\n"
            "  dmb sy\n"
            "  cbz %[stack], 1f\n"
            "  mov sp, %[stack]\n"
            "1:br %[entry]\n"
            :
            : [entry] "r" (entry_point),
            [stack] "r" (stack),
            [info]  "r" (stivale_struct)
            : "x0"
        );
    } else {
        asm volatile (
            "  mov x0, %[info]\n"
            "  msr SPSel, XZR\n"
            "  dmb sy\n"
            "  cbz %[stack], 1f\n"
            "  mov sp, %[stack]\n"
            "1:br %[entry]\n"
            :
            : [entry] "r" (entry_point),
            [stack] "r" (stack),
            [info]  "r" (stivale_struct)
            : "x0"
        );
    }
    __builtin_unreachable();
}
