OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

    .entry : {
        *(.entry*)
    }

    .realmode : {
        *(.realmode*)
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
        *(.stage3_entry*)
        *(.stage3_build_id*)
        *(.stage3_text*)
    }

    .stage3.data : {
        *(.stage3_data*)
    }

    .map : {
        *(.map*)
        limine_sys_size = . - 0x8000;
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
