#include <yak/log.h>
#include <yak/sched.h>
#include <yak/syscall.h>
#include <yak-abi/vm-flags.h>

DEFINE_SYSCALL(SYS_MMAP, mmap, unsigned long addr, unsigned long len,
	       unsigned long prot, unsigned long flags, unsigned long fd,
	       unsigned long pgoff)
{
	pr_warn("sys_mmap is a stub\n");
	sched_exit_self();
	return 0;
}
