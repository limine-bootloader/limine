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
        *(.rodata .rodata.*)
        *(.data .data.*)
    } :data

    .bss : {
        bss_begin = .;
        *(COMMON)
        *(.bss .bss.*)
        bss_end = .;
    } :data

    /DISCARD/ : {
        *(.note .note.*)
    }
}
