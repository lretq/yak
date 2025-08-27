#include <yak/syscall.h>

void _start()
{

	for (;;) {
		__syscall1(SYS_DEBUG_LOG, (uint64_t)"Hello World!");
		__syscall1(SYS_DEBUG_SLEEP, 5UL * 1000 * 1000 * 1000);
	}
}
