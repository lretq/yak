#include <yak/syscall.h>
#include <yak/log.h>
#include <yak/types.h>
#include <yak/timer.h>

DEFINE_SYSCALL(SYS_DEBUG_LOG, debug_log, const char *msg)
{
	pr_debug("sys_debug_log: %s\n", msg);
	return 0;
}

DEFINE_SYSCALL(SYS_DEBUG_SLEEP, debug_sleep, nstime_t duration)
{
	ksleep(duration);
	return 0;
}
