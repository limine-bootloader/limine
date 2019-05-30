OUTPUT_FORMAT(binary)
ENTRY(main)

SECTIONS
{
	. = 0x7e00;

    .text : {
        bootsect_begin = .;
        KEEP(*(.entry*))
        KEEP(*(.text*))
    }

    .data : {
        KEEP(*(.data*))
        KEEP(*(.bss*))
        . += 3584 - (. - bootsect_begin);
    }

}
