.extern kstart

.text

	.globl _start
	.balign 16
	.type _start, @function
_start:
	# switch off of limine's stack
	# we want to reclaim bootloader memory
	lea __init_stack_top(%rip), %rsp
	jmp kstart 

.bss

	.balign 16

	.skip 16384
	.type __init_stack_top, @object
	.globl __init_stack_top
__init_stack_top:
