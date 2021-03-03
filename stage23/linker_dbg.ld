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
        *libgcc.a:*(.text*)
    }

    .stage2.data : {
        *.s2.o(.data*)
        *.s2.o(.rodata*)
        *libgcc.a:*(.data*)
        *libgcc.a:*(.rodata*)
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
    }

    .map : {
        limine_map = .;
        limine_sys_size = .;
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss*)
        bss_end = .;
    }
}
