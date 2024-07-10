PHDRS
{
    text    PT_LOAD    FLAGS(0x05); /* Execute + Read */
    rodata  PT_LOAD    FLAGS(0x04); /* Read only */
    data    PT_LOAD    FLAGS(0x06); /* Write + Read */
    dynamic PT_DYNAMIC FLAGS(0x06); /* Dynamic segment needed for PIE */
}

SECTIONS
{
    . = SIZEOF_HEADERS;
    kernel_start = . - SIZEOF_HEADERS;

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
