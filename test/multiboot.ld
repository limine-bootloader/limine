ENTRY(_start)

SECTIONS {
    . = 0x1000;

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
        *(.rodata.*)
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
