ENTRY(stivale_main)

PHDRS {
  none   PT_NULL FLAGS(0);
  text   PT_LOAD FLAGS((1 << 2) | (1 << 0) /* Readable | Executable */);
  data   PT_LOAD FLAGS((1 << 2) | (1 << 1) /* Readable | Writeable  */);
  rodata PT_LOAD FLAGS((1 << 2)            /* Readable */);
}

SECTIONS
{
    . = 0xffffffff80100000;

    .stivalehdr ALIGN(4K) :
    {
        KEEP(*(.stivalehdr))
    }:none

    .stivale2hdr ALIGN(4K) :
    {
        KEEP(*(.stivale2hdr))
    }

    .text ALIGN(4K) :
    {
        KEEP(*(.text*))
    }:text

    .rodata ALIGN(4K) :
    {
        KEEP(*(.rodata*))
    }:rodata

    .data ALIGN(4K) :
    {
        KEEP(*(.data*))
    }:data

    .bss ALIGN(4K) :
    {
        KEEP(*(COMMON))
        KEEP(*(.bss*))
    }
}
