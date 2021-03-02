OUTPUT_FORMAT(elf64-x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 4K;

    .text : {
        *(.text*)
        *(.realmode*)
    }

    .rodata : {
        limine_map = .;
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    .reloc : {
        LONG(0);
        LONG(10);
        SHORT(0);
    }

    /DISCARD/ : {
        *(*)
    }
}
