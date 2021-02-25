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

    .stage2.text : {
        *(.text*)
    }

    .stage2.data : {
        *(.data*)
        *(.rodata*)
    }

    .stage3.text : {
        stage3_addr = .;
        *(.stage3_text*)
    }

    .stage3.data : {
        *(.stage3_data*)
    }

    .map : {
        KEEP(*(.map*))
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
