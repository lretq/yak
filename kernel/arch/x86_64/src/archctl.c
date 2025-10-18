#include <yak/cpudata.h>
#include <yak/syscall.h>
#include <yak/log.h>
#include <yak-abi/errno.h>

#include "asm.h"

DEFINE_SYSCALL(SYS_ARCHCTL, archctl, int op, unsigned long addr)
{
	switch (op) {
	case ARCHCTL_SET_FSBASE:
		curthread()->pcb.fsbase = addr;
		wrmsr(MSR_FSBASE, addr);
		break;
	case ARCHCTL_SET_GSBASE:
		curthread()->pcb.gsbase = addr;
		wrmsr(MSR_KERNEL_GSBASE, addr);
		break;
	default:
		pr_warn("archctl unknown op: %d, addr: %ld (sizeof %ld)\n", op,
			addr, sizeof(addr));
		return -EINVAL;
	}
	return 0;
}
