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

    .symtab 0 : {
        *(.symtab)
    }

    .strtab 0 : {
        *(.strtab)
    }

    .shstrtab 0 : {
        *(.shstrtab)
    }

    .debug_aranges 0 : {
        *(.debug_aranges)
    }

    .debug_pubnames 0 : {
        *(.debug_pubnames)
    }

    .debug_info 0 : {
        *(.debug_info)
    }

    .debug_abbrev 0 : {
        *(.debug_abbrev)
    }

    .debug_line 0 : {
        *(.debug_line)
    }

    .debug_loclists 0 : {
        *(.debug_loclists)
    }

    .debug_rnglists 0 : {
        *(.debug_rnglists)
    }

    .debug_frame 0 : {
        *(.debug_frame)
    }

    .debug_loc 0 : {
        *(.debug_loc)
    }

    .debug_ranges 0 : {
        *(.debug_ranges)
    }

    .debug_str 0 : {
        *(.debug_str)
    }

    .debug_line_str 0 : {
        *(.debug_line_str)
    }

    /DISCARD/ : {
        *(*)
    }
}
