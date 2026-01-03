ENTRY(_start)

SECTIONS
{
    /* Link at 3GB + 32KB, Load at 32KB (0x8000) */
    . = 0xC0008000;

    .text : AT(ADDR(.text) - 0xC0000000)
    {
        _text_start = .;
        *(.text)
        . = ALIGN(4096);
        _text_end = .;
    }

    .rodata : AT(ADDR(.rodata) - 0xC0000000)
    {
        _rodata_start = .;
        *(.rodata*)
        . = ALIGN(4096);
        _rodata_end = .;
    }

    /* C++ Global Constructors */
    .init_array : AT(ADDR(.init_array) - 0xC0000000)
    {
        __init_array_start = .;
        KEEP(*(.init_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        __init_array_end = .;
    }

    /* C++ Global Destructors */
    .fini_array : AT(ADDR(.fini_array) - 0xC0000000)
    {
        __fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        __fini_array_end = .;
    }

    .data : AT(ADDR(.data) - 0xC0000000)
    {
        _data_start = .;
        *(.data)
        . = ALIGN(4096);
        _data_end = .;
    }

    .bss : AT(ADDR(.bss) - 0xC0000000)
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4096);
        _bss_end = .;
    }

    _kernel_end = .;
}
