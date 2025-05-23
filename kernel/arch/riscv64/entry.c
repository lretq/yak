extern void _start()
{
	asm volatile("wfi");
	asm volatile("nop");
}
