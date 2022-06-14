ENTRY(limine_main)

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
    dynamic PT_DYNAMIC FLAGS((1 << 1) | (1 << 2)) ; /* Dynamic segment needed for PIE */
}

SECTIONS
{
    . = 0xffffffff80000000;
    kernel_start = .;

    .text : {
        *(.text*)
    } :text

    . += 0x1000;

    .limine_reqs : {
        KEEP(*(.limine_reqs))
        QUAD(0)
    } :rodata

    .rodata : {
        *(.rodata*)
    } :rodata

    . += 0x1000;

    .data : {
        *(.data*)
    } :data

    .dynamic : {
        *(.dynamic)
    } :data :dynamic

    .bss : {
        *(COMMON)
        *(.bss*)
    } :data
}
