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
        stage3_addr = .;
        *(.stage3_build_id*)
        limine_map = .;
        limine_sys_size = .;
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
