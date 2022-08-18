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
