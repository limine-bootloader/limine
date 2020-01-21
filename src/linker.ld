OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
    . = 0x8000;

    .text : {
        bootsect_begin = .;
        KEEP(*(.entry*))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
        . += 31744 - (. - bootsect_begin);  /* Limit how big the thing can get
                                               31744 = 32768 - (512 * 2)
                                               32KiB of MBR gap minus 2 sectors
                                               for stages 1 and 2 */
    }

    .bss : {
        bss_begin = .;
        *(.bss*)
        bss_end = .;
    }
}
