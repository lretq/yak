void syscall_test()
{
	asm volatile("syscall" ::: "memory", "r11", "rcx");
}

void _start()
{
	syscall_test();
	for (;;) {
	syscall_test();
		asm volatile("nop");
	}
}
