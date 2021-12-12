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

    .stage2.build-id : {
        build_id_s2 = .;
        *build-id.s2.o(*)
    }

    .stage2.data : {
        *.s2.o(.data*)
        *.s2.o(.rodata*)
        *(.stage2_map)
    }

    .stage3.text : {
        stage3_addr = .;
        *(.text*)
    }

    .stage3.build-id : {
        build_id_s3 = .;
        *build-id.s3.o(*)
    }

    .stage3.data : {
        *(.rodata*)
        *(.full_map*)
        *(.no_unwind*)
        data_begin = .;
        *(.data*)
        limine_sys_size = . - 0x8000;
    }

    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss*)
        bss_end = .;
        data_end = .;
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
