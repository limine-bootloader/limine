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
        stage2_map = .;
        stage3_addr = .;
        *(.stage3_build_id*)
        full_map = .;
        limine_sys_size = .;
        getchar_internal = .;
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss*)
        bss_end = .;
    }

    .symtab 0 : {
        *(.symtab)
    }

    .strtab 0 : {
        *(.strtab)
    }

    .shstrtab 0 : {
        *(.shstrtab)
    }

    /DISCARD/ : {
        *(*)
    }
}
