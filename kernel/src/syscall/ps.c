#include <stdint.h>
#include <yak/log.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/syscall.h>
#include <yak/timespec.h>
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

DEFINE_SYSCALL(SYS_SLEEP, sleep, struct timespec *req, struct timespec *rem)
{
	nstime_t sleep_ns = STIME(req->tv_sec) + req->tv_nsec;
	pr_debug("sys_sleep() for %lx ns\n", sleep_ns);
	nstime_t start = plat_getnanos();
	ksleep(sleep_ns);
	nstime_t time_passed = plat_getnanos() - start;

	if (rem) {
		nstime_t remaining =
			(sleep_ns > time_passed) ? (sleep_ns - time_passed) : 0;
		rem->tv_sec = remaining / STIME(1);
		rem->tv_nsec = remaining % STIME(1);
	}

	return SYS_OK(0);
}
