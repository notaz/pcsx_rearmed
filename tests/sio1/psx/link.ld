ENTRY(_start)
SECTIONS
{
	. = 0x80010000;
	.text : { *(.text.start) *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) *(.sdata*) }
	.bss : { *(.sbss*) *(.bss*) *(COMMON) }
	/DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug*) *(.note*) }
}
