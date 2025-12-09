#include <yak/process.h>
#include <yak/queue.h>
#include <yak/spinlock.h>
#include <yak/types.h>
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

DEFINE_SYSCALL(SYS_SETSID, setsid)
{
	struct kprocess *proc = curproc();
	ipl_t ipl = spinlock_lock(&proc->jobctl_lock);

	if (proc->pgrp_leader == NULL) {
		spinlock_unlock(&proc->jobctl_lock, ipl);
		return SYS_ERR(EPERM);
	}

	struct kprocess *pgrp_lead = proc->pgrp_leader;
	spinlock_lock_noipl(&pgrp_lead->pgrp_lock);
	TAILQ_REMOVE(&pgrp_lead->pgrp_members, proc, pgrp_entry);
	spinlock_unlock_noipl(&pgrp_lead->pgrp_lock);

	proc->pgrp_leader = NULL;
	TAILQ_INIT(&proc->pgrp_members);

	proc->session.leader = NULL;
	proc->session.ctty = NULL;

	spinlock_unlock(&proc->jobctl_lock, ipl);
	return SYS_OK(proc->pid);
}

DEFINE_SYSCALL(SYS_SETPGID, setpgid, pid_t pid, pid_t pgid)
{
	if (pgid < 0) {
		return SYS_ERR(EINVAL);
	}

	struct kprocess *cur_proc = curproc();
	struct kprocess *proc;
	if (pid == 0) {
		proc = cur_proc;
	} else {
		proc = pid_to_proc(pid);
		if (!proc)
			return SYS_ERR(ESRCH);
	}

	if (proc != cur_proc && proc->parent_process != cur_proc) {
		return SYS_ERR(ESRCH);
	}

	struct kprocess *pgrp;
	if (pgid == 0) {
		pgrp = cur_proc;
	} else {
		pgrp = pid_to_proc(pgid);
	}

	if (pgrp == NULL)
		return SYS_ERR(EPERM);

	ipl_t ipl = spinlock_lock(&proc->jobctl_lock);

	if (pgid == 0 || pgid == pid) {
		if (proc->pgrp_leader) {
			spinlock_lock_noipl(&proc->pgrp_leader->pgrp_lock);
			TAILQ_REMOVE(&proc->pgrp_leader->pgrp_members, proc,
				     pgrp_entry);
			spinlock_unlock_noipl(&proc->pgrp_leader->pgrp_lock);
		}

		proc->pgrp_leader = NULL;
		TAILQ_INIT(&proc->pgrp_members);
	} else {
		spinlock_lock_noipl(&pgrp->pgrp_lock);
		proc->pgrp_leader = pgrp;
		TAILQ_INSERT_HEAD(&pgrp->pgrp_members, proc, pgrp_entry);
		spinlock_unlock_noipl(&pgrp->pgrp_lock);
	}

	spinlock_unlock(&proc->jobctl_lock, ipl);

	return SYS_OK(0);
}

DEFINE_SYSCALL(SYS_GETPGID, getpgid, pid_t pid)
{
	struct kprocess *proc;
	if (pid == 0) {
		proc = curproc();
	} else {
		proc = pid_to_proc(pid);
		if (!proc)
			return SYS_ERR(ESRCH);
	}

	ipl_t ipl = spinlock_lock(&proc->jobctl_lock);
	struct kprocess *leader_proc = proc->pgrp_leader;
	pid_t pgid = leader_proc ? leader_proc->pid : proc->pid;
	spinlock_unlock(&proc->jobctl_lock, ipl);

	return SYS_OK(pgid);
}

DEFINE_SYSCALL(SYS_GETSID, getsid, pid_t pid)
{
	struct kprocess *proc;
	if (pid == 0) {
		proc = curproc();
	} else {
		proc = pid_to_proc(pid);
		if (!proc)
			return SYS_ERR(ESRCH);
	}

	ipl_t ipl = spinlock_lock(&proc->jobctl_lock);
	struct kprocess *leader_proc = proc->session.leader;
	pid_t sid = leader_proc ? leader_proc->pid : proc->pid;
	spinlock_unlock(&proc->jobctl_lock, ipl);

	return SYS_OK(sid);
}

DEFINE_SYSCALL(SYS_SLEEP, sleep, struct timespec *req, struct timespec *rem)
{
	nstime_t sleep_ns = STIME(req->tv_sec) + req->tv_nsec;
	pr_extra_debug("sys_sleep() for %lx ns\n", sleep_ns);
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
