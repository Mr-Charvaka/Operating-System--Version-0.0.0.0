ENTRY(main)
SECTIONS {
    . = 0xC000;
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss  : { *(.bss)  }
    _kernel_end = .;
}
