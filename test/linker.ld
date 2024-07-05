SECTIONS
{
    . = SIZEOF_HEADERS;
    kernel_start = .;

    .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.rodata .rodata.*)
    }

    .text ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.text .text.*)
    }

    .data ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.data .data.*)

        *(.limine_requests_start_marker)
        *(.limine_requests)
        *(.limine_requests_end_marker)

        *(.sdata .sdata.*)
    }

    .dynamic : {
        *(.dynamic)
    }

    .bss ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.interp)
    }
}
