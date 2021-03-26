ENTRY(stivale_main)

SECTIONS
{
    . = 0xffffffff80100000;

    .stivalehdr ALIGN(4K) :
    {
        *(.stivalehdr)
    }

    .stivale2hdr ALIGN(4K) :
    {
        *(.stivale2hdr)
    }

    .text ALIGN(4K) :
    {
        *(.text*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    .data ALIGN(4K) :
    {
        *(.data*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
}
