ENTRY(_start)

SECTIONS
{
    . = 0x87800000;

    .text :
    {
        KEEP(*(.text._start))
        KEEP(start.o(.text*))
        *(.text*)
    }

    .rodata ALIGN(4) :
    {
        *(.rodata*)
    }

    .data ALIGN(4) :
    {
        *(.data*)
    }

    . = ALIGN(4);
    __bss_start = .;

    .bss ALIGN(4) :
    {
        *(.bss*)
        *(COMMON)
    }

    . = ALIGN(4);
    __bss_end = .;
}