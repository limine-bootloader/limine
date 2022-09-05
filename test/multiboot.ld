ENTRY(_start)

SECTIONS {
    . = 1M;

    .boot :
    {
        /* Ensure that the multiboot header is at the beginning! */
        *(.multiboot_header)
    }

    . = ALIGN(4K);
    .text :
    {
        *(.text .text.*)
    }

    . = ALIGN(4K);
    .rodata :
    {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4K);
    .data :
    {
        *(.data .data.*)
    }

    . = ALIGN(4K);
    .bss :
    {
        *(.bss .bss.*)
    }
}
