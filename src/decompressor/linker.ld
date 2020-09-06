OUTPUT_FORMAT(elf32-i386)
ENTRY(main)

SECTIONS
{
    . = 0x70000;

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
