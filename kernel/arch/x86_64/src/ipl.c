#include <yak/ipl.h>
#include <stdint.h>

ipl_t curipl()
{
	uint64_t current;
	asm volatile("mov %%cr8, %0" : "=r"(current));
	return current;
}

void setipl(ipl_t ipl)
{
	asm volatile("mov %0, %%cr8" ::"r"((uint64_t)ipl));
}
