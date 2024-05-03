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
        *(.text .text.*)
    } :text

    . += 0x1000;

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . += 0x1000;

    .data : {
        *(.data .data.*)

        *(.limine_requests_start_marker)
        *(.limine_requests)
        *(.limine_requests_end_marker)

        *(.sdata .sdata.*)
    } :data

    .dynamic : {
        *(.dynamic)
    } :data :dynamic

    .bss : {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    } :data
}
