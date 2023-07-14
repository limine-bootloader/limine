OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

PHDRS
{
    text  PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ;
    data  PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ;
}

SECTIONS
{
    . = 0x70000;

    .text : {
        *(.entry)
        *(.text .text.*)
    } :text

    .data : {
        *(.data .data.*)
        *(.rodata .rodata.*)
    } :data

    .bss : {
        bss_begin = .;
        *(.bss .bss.*)
        *(COMMON)
        bss_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
