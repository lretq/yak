#include <stddef.h>
#include <yak/syscall.h>
#include <yak-abi/errno.h>
#include <yak/log.h>

#define SYSCALL_LIST                        \
	X(SYS_DEBUG_SLEEP, sys_debug_sleep) \
	X(SYS_DEBUG_LOG, sys_debug_log)     \
	X(SYS_WRITE, sys_write)             \
	X(SYS_READ, sys_read)               \
	X(SYS_CLOSE, sys_close)             \
	X(SYS_OPEN, sys_open)               \
	X(SYS_SEEK, sys_seek)               \
	X(SYS_ARCHCTL, sys_archctl)

#if 0
#define X(num, fn)                              \
	[[gnu::weak]]                           \
	long fn()                               \
	{                                       \
		pr_warn("sys_noop(%d)\n", num); \
		return -ENOSYS;                 \
	}
SYSCALL_LIST
#undef X
#endif

#define X(num, fn) extern long fn();
SYSCALL_LIST
#undef X

#define X(num, fn) [num] = (void *)fn,
syscall_fn syscall_table[MAX_SYSCALLS] = { SYSCALL_LIST };
#undef X

long sys_noop()
{
	pr_warn("sys_noop called\n");
	return -ENOSYS;
}

void syscall_init()
{
	for (size_t i = 0; i < MAX_SYSCALLS; i++) {
		syscall_table[i] = (void *)sys_noop;
	}

#define X(num, fn) syscall_table[num] = (void *)fn;
	SYSCALL_LIST
#undef X
}
