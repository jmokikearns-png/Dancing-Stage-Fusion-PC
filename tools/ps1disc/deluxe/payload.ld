/* In the executable's zero block 0x80098AA8..0x8009DC04, 16-aligned (the
 * output section takes its inputs' alignment). */
SECTIONS
{
	. = 0x80098AB0;
	.blob : {
		dlx_blob_start = .;
		*entry.o(.text)
		*(.text*)
		*(.rodata*)
		*(.data*)
		*(.sdata*)
		*(.sbss*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
	}
	/DISCARD/ : { *(.comment) *(.pdr) *(.MIPS.*) *(.reginfo) *(.gnu.attributes) *(.note*) }
}
