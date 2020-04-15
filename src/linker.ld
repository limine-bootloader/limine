OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
    . = 0x8000;

    .text : {
        bootsect_begin = .;
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
