OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
	. = 0x7c00;

    .text : {
        bootsect_begin = .;
        KEEP(*(.early_boot*))
        KEEP(*(.text*))
    }

    .data : {
        KEEP(*(.data*))
        KEEP(*(.bss*))
        . += 510 - (. - bootsect_begin);
        SHORT(0xaa55)
    }

}
