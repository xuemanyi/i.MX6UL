ENTRY(_start)

SECTIONS
{
	. = 0x87800000;

	.text :
	{
		KEEP(*(.text._start))
		*(.text)
		*(.text.*)
	}

	.rodata ALIGN(4) :
	{
		*(.rodata)
		*(.rodata.*)
	}

	.data ALIGN(4) :
	{
		*(.data)
		*(.data.*)
	}

	. = ALIGN(4);
	__bss_start = .;

	.bss ALIGN(4) :
	{
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}

	. = ALIGN(4);
	__bss_end = .;
}