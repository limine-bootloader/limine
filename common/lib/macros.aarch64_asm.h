// Branch to \el1 if in EL1, or to \el2 if in EL2
// Uses \reg, halts if not in EL1 or EL2
.macro PICK_EL reg, el1, el2
    mrs \reg, currentel
    and \reg, \reg, #0b1100

    cmp \reg, #0b0100 // EL1?
    b.eq \el1
    cmp \reg, #0b1000 // EL2?
    b.eq \el2

    // Halt otherwise
    msr daifset, #0b1111
99:
    wfi
    b 99b
.endm


// Zero out all general purpose registers apart from X0
.macro ZERO_REGS_EXCEPT_X0
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    mov x4, xzr
    mov x5, xzr
    mov x6, xzr
    mov x7, xzr
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mov x12, xzr
    mov x13, xzr
    mov x14, xzr
    mov x15, xzr
    mov x16, xzr
    mov x17, xzr
    mov x18, xzr
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr
    mov x27, xzr
    mov x28, xzr
    mov x29, xzr
    mov x30, xzr
.endm

// Configure EL2 to neither trap nor perturb EL1 before dropping there. Every
// field written is architecturally UNKNOWN out of reset. Armv8.0 only: later
// extensions add controls this misses, and CPTR_EL2 bits 12 and 8 stop being
// RES1 under FEAT_SME and FEAT_SVE.
.macro INIT_EL2_FOR_EL1 tmp1, tmp2
    // Let EL1 and EL0 reach the counters and the physical timer.
    mov \tmp1, #3
    msr cnthctl_el2, \tmp1
    msr cntvoff_el2, xzr

    // EL1 reads of MIDR_EL1 and MPIDR_EL1 are served by these instead.
    mrs \tmp1, midr_el1
    msr vpidr_el2, \tmp1
    mrs \tmp1, mpidr_el1
    msr vmpidr_el2, \tmp1

    // Stage 2 stays off, but EL1&0 TLB entries are tagged with the VMID here.
    msr vttbr_el2, xzr

    // Don't trap FP/SIMD, the trace registers, or CPACR_EL1 itself.
    mov \tmp1, #0x33ff
    msr cptr_el2, \tmp1

    // Don't trap AArch32 CP15 accesses.
    msr hstr_el2, xzr

    // Clear the debug and PMU traps, and give EL1 every counter through HPMN.
    // PMCR_EL0 only exists under FEAT_PMUv3, and PMUVer 0xf reports no count.
    mov \tmp2, xzr
    mrs \tmp1, id_aa64dfr0_el1
    ubfx \tmp1, \tmp1, #8, #4
    cbz \tmp1, .Lno_pmu_\@
    cmp \tmp1, #0xf
    b.eq .Lno_pmu_\@
    mrs \tmp2, pmcr_el0
    ubfx \tmp2, \tmp2, #11, #5
.Lno_pmu_\@:
    msr mdcr_el2, \tmp2

    // RW for AArch64 at EL1, SWIO, and EnSCXT, whose 0 traps SCXTNUM_EL0 and
    // SCXTNUM_EL1 under FEAT_CSV2_2 or FEAT_CSV2_1p2, both optional from
    // Armv8.0. Everything else, TGE included, stays 0.
    mov \tmp1, xzr
    orr \tmp1, \tmp1, #(1 << 53)
    orr \tmp1, \tmp1, #(1 << 31)
    orr \tmp1, \tmp1, #(1 << 1)
    msr hcr_el2, \tmp1

    // EL1 accesses to the GICv3 CPU interface trap unless SRE and Enable are
    // set, and the write only sticks where EL3 enabled the interface in turn.
    mrs \tmp1, id_aa64pfr0_el1
    ubfx \tmp1, \tmp1, #24, #4
    cbz \tmp1, .Lno_gicv3_\@
    mrs \tmp1, icc_sre_el2
    orr \tmp1, \tmp1, #(1 << 3)
    orr \tmp1, \tmp1, #(1 << 0)
    msr icc_sre_el2, \tmp1
    isb
    mrs \tmp1, icc_sre_el2
    tbz \tmp1, #0, .Lno_gicv3_\@
    msr ich_hcr_el2, xzr
.Lno_gicv3_\@:

    isb
.endm
