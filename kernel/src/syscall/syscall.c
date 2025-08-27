#include <stddef.h>
#include <yak/syscall.h>
#include <yak/log.h>

#define SYSCALL_LIST                    \
	X(SYS_ARCHCTL, sys_archctl)     \
	X(SYS_DEBUG_LOG, sys_debug_log) \
	X(SYS_DEBUG_SLEEP, sys_debug_sleep)

#define X(num, fn) extern long fn();
SYSCALL_LIST
#undef X

syscall_fn syscall_table[MAX_SYSCALLS];

long sys_noop()
{
	pr_warn("sys_noop called\n");
	return -1;
}

void syscall_init()
{
	for (size_t i = 0; i < MAX_SYSCALLS; i++) {
		syscall_table[i] = sys_noop;
	}
#define X(num, fn) syscall_table[num] = fn;
	SYSCALL_LIST;
#undef X
}
