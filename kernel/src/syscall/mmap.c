#include "yak/arch-mm.h"
#include "yak/ipl.h"
#include "yak/process.h"
#include "yak/vm/map.h"
#include "yak/vmflags.h"
#include <yak/log.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/syscall.h>
#include <yak-abi/vm-flags.h>

DEFINE_SYSCALL(SYS_MMAP, mmap, void *hint, unsigned long len,
	       unsigned long prot, unsigned long flags, unsigned long fd,
	       unsigned long pgoff)
{
	pr_debug(
		"mmap(hint=%p, len=%lu, prot=%lu, flags=%lu, fd=%lu, pgoff=%lu)\n",
		hint, len, prot, flags, fd, pgoff);

	if (flags == MAP_FILE)
		printk(0, "MAP_FILE ");
	else {
		if (flags & MAP_SHARED)
			printk(0, "MAP_SHARED ");
		if (flags & MAP_PRIVATE)
			printk(0, "MAP_PRIVATE ");
		if (flags & MAP_FIXED)
			printk(0, "MAP_FIXED ");
		if (flags & MAP_ANONYMOUS)
			printk(0, "MAP_ANONYMOUS ");
	}
	printk(0, "\n");

	struct kprocess *proc = curproc();

	vm_prot_t vm_prot = VM_USER;
	if (prot & PROT_READ)
		vm_prot |= VM_READ;
	if (prot & PROT_WRITE)
		vm_prot |= VM_WRITE;
	if (prot & PROT_EXEC)
		vm_prot |= VM_EXECUTE;

	vm_prot |= VM_USER | VM_RW | VM_EXECUTE;

	assert(prot != PROT_NONE);

	vm_inheritance_t inheritance = VM_INHERIT_SHARED;
	if (flags & MAP_PRIVATE) {
		// CoW mapping
		inheritance = VM_INHERIT_COPY;
	}

	int vmflags = 0;
	if (flags & MAP_FIXED) {
		vmflags |= VM_MAP_FIXED;
		vmflags |= VM_MAP_OVERWRITE;
	}
	assert((flags & MAP_FILE) == 0);

	vaddr_t out = 0;
	status_t rv = vm_map(&proc->map, NULL, len, 0, vm_prot, inheritance,
			     VM_CACHE_DEFAULT, (vaddr_t)hint, vmflags, &out);
	IF_ERR(rv) return rv;

	pr_warn("sys_mmap is a stub (%lx)\n", out);
	return out;
}
