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
        *.s2.o(.text*)
    }

    .stage2.data : {
        *.s2.o(.data*)
        *.s2.o(.rodata*)
        *(.stage2_map)
    }

    .stage3.text : {
        stage3_addr = .;
        *(.stage3_entry*)
        *(.stage3_build_id*)
        *(.text*)
    }

    .stage3.data : {
        *(.data*)
        *(.rodata*)
        *(.full_map*)
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
