OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x70000;

    .text : {
        *(.entry)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss .bss.*)
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
