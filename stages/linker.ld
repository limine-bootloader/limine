OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

    .entry : {
        KEEP(*(.entry*))
    }

    .realmode : {
        KEEP(*(.realmode*))
    }

    .stage2 : {
        *(.text*)
        *(.data*)
        *(.rodata*)
    }

    .stage3 : {
        stage3_addr = .;
        *(.stage3_text*)
        *(.stage3_data*)
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss*)
        bss_end = .;
    }

    /DISCARD/ : {
        *(*)
    }
}
