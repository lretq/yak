OUTPUT_FORMAT(elf64-x86-64)

ENTRY(_start)

PHDRS
{
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
	    . = 0x1000;
	.text : {
        *(.text .text.*)
    } :text
		    . = ALIGN(CONSTANT(MAXPAGESIZE));

	.rodata : {
        *(.rodata .rodata.*)
    } :rodata

		 .init_array : {
        __init_array = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;
    } :rodata

		    . = ALIGN(CONSTANT(MAXPAGESIZE));

	 .data : {
        *(.data .data.*)
    } :data
}
