#include <yak/log.h>
#include <yak/sched.h>
#include <yak/syscall.h>
#include <yak-abi/syscall.h>

DEFINE_SYSCALL(SYS_EXIT, exit, int rc)
{
	pr_debug("sys_exit() is a stub: exit with code %d\n", rc);
	sched_exit_self();
}
