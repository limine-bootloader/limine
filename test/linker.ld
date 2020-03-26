ENTRY(_start)

SECTIONS {
    . = 0xffffffff80100000;

    .stivalehdr : ALIGN(4K) {
        *(.stivalehdr)
    }

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    }
}
