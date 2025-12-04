#include <stddef.h>
#include <yak/types.h>
#include <yak/syscall.h>
#include <yak/macro.h>
#include <yak/vm.h>
#include <yak/cpudata.h>
#include <yak/log.h>
#include <yak-abi/errno.h>
#include <yak-abi/vm-flags.h>

DEFINE_SYSCALL(SYS_MUNMAP, munmap, void *addr, size_t length)
{
	struct kprocess *proc = curproc();

	if (!IS_ALIGNED_POW2((vaddr_t)addr, PAGE_SIZE)) {
		return SYS_ERR(EINVAL);
	}

	// this splits any existing mappings and should correctly
	// deref and free pages in use
	vm_unmap(&proc->map, (vaddr_t)addr, length, 0);

	return SYS_OK(0);
}

DEFINE_SYSCALL(SYS_MMAP, mmap, void *hint, unsigned long len,
	       unsigned long prot, unsigned long flags, unsigned long fd,
	       unsigned long pgoff)
{
#if 0
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
#endif

	struct kprocess *proc = curproc();

	vm_prot_t vm_prot = VM_USER;
	if (prot & PROT_READ)
		vm_prot |= VM_READ;
	if (prot & PROT_WRITE)
		vm_prot |= VM_WRITE;
	if (prot & PROT_EXEC)
		vm_prot |= VM_EXECUTE;

	vm_prot |= VM_USER | VM_RW | VM_EXECUTE;

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

	status_t rv;
	vaddr_t out = 0;

	if (prot == PROT_NONE) {
		rv = vm_map_reserve(&proc->map, (vaddr_t)hint, len, vmflags,
				    &out);
	} else {
		rv = vm_map(&proc->map, NULL, len, 0, vm_prot, inheritance,
			    VM_CACHE_DEFAULT, (vaddr_t)hint, vmflags, &out);
	}

	RET_ERRNO_ON_ERR(rv);

	return SYS_OK(out);
}
