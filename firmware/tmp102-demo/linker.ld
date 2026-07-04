ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x8000, LENGTH = 128M - 0x8000
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    . = ALIGN(8);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
