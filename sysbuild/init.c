#include <yak/syscall.h>

void _start()
{
	__syscall2(SYS_ARCHCTL, ARCHCTL_SET_FSBASE, 0xB00B5);

	for (;;) {
		__syscall1(SYS_DEBUG_LOG, (uint64_t)"Hello World!");
		__syscall1(SYS_DEBUG_SLEEP, 5UL * 1000 * 1000 * 1000);
	}
}
