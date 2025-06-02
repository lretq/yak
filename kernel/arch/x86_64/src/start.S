.extern limine_start

.text

	.globl _start
	.balign 16
	.type _start, @function
_start:
	# switch off of limine's stack
	# we want to reclaim bootloader memory
	lea __init_stack_top(%rip), %rsp
	jmp limine_start

.bss

	.balign 16

	.skip 8192
	.type __init_stack_top, @object
__init_stack_top:
