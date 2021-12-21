OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

    .entry : {
        *(.entry)
    }

    .realmode : {
        *(.realmode)
    }

    .stage2.text : {
        *.s2.o(.text .text.*)
    }

    .stage2.build-id : {
        build_id_s2 = .;
        *build-id.s2.o(*)
    }

    .stage2.data : {
        *.s2.o(.no_unwind)
        s2_data_begin = .;
        *.s2.o(.data .data.*)
        s2_data_end = .;
        *.s2.o(.rodata .rodata.*)
        stage2_map = .;
        stage3_common = .;
        build_id_s3 = .;
        full_map = .;
        getchar_internal = .;
        getchar = .;
        menu = .;
        stage3_addr = .;
        data_begin = .;
    }

    .note.gnu.build-id : {
        *(.note.gnu.build-id)
        limine_sys_size = . - 0x8000;
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss .bss.*)
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
