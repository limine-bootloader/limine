OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
    . = 0x60000;

    .text : {
        KEEP(*(.entry*))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss*)
        bss_end = .;
    }
}
