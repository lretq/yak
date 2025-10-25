#include <yak/log.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/syscall.h>
#include <yak-abi/errno.h>
#include <yak-abi/syscall.h>

DEFINE_SYSCALL(SYS_EXIT, exit, int rc)
{
	pr_debug("sys_exit() is a stub: exit with code %d\n", rc);
	sched_exit_self();
}

DEFINE_SYSCALL(SYS_GETPID, getpid)
{
	return SYS_OK(curproc()->pid);
}

DEFINE_SYSCALL(SYS_GETPPID, getppid)
{
	struct kprocess *parent = curproc()->parent_process;
	return SYS_OK(parent ? parent->pid : 0);
}
