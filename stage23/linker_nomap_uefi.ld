OUTPUT_FORMAT(elf64-x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 4K;

    .text : {
        *(.text*)
    }

    .data : {
        *(.realmode*)
        *(.data*)
    }

    .rodata : {
        *(.rodata*)
    }

    .map : {
        limine_map = .;
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(*)
    }
}
