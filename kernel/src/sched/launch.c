#include <stdint.h>
#include <string.h>
#include <yak/vm/map.h>
#include <yak/fs/vfs.h>
#include <yak/sched.h>
#include <yak/elf.h>
#include <yak/macro.h>
#include <yak/heap.h>

static uintptr_t elf_load(struct vnode *vn, struct kprocess *process)
{
	vm_map_tmp_switch(&process->map);

	Elf64_Ehdr ehdr;
	size_t len = sizeof(Elf64_Ehdr);
	EXPECT(vfs_read(vn, 0, &ehdr, &len));

	size_t pos = ehdr.e_phoff;

	for (size_t i = 0; i < ehdr.e_phnum; i++) {
		pos = ehdr.e_phoff + i * ehdr.e_phentsize;

		Elf64_Phdr phdr;
		len = sizeof(Elf64_Phdr);
		EXPECT(vfs_read(vn, pos, &phdr, &len));

		if (phdr.p_type == PT_LOAD) {
			size_t base = ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE);
			size_t page_off = phdr.p_vaddr - base;
			size_t size =
				ALIGN_UP(page_off + phdr.p_memsz, PAGE_SIZE);
			if (size == 0)
				continue;

			vaddr_t seg_addr = base;

			EXPECT(vm_map(&process->map, NULL, size, 0, 1,
				      VM_RW | VM_USER | VM_EXECUTE,
				      VM_INHERIT_SHARED, VM_CACHE_DEFAULT,
				      &seg_addr));

			for (size_t i = 0; i < size; i += PAGE_SIZE) {
				size_t size = phdr.p_filesz;
				EXPECT(vfs_read(vn, phdr.p_offset,
						(void *)(phdr.p_vaddr), &size));
			}

			memset((void *)(phdr.p_vaddr + phdr.p_filesz), 0,
			       phdr.p_memsz - phdr.p_filesz);
		}
	}

	vm_map_tmp_disable();

	return ehdr.e_entry;
}

status_t sched_launch(char *path, int priority)
{
	struct vnode *vn;
	status_t status = vfs_open(path, &vn);
	IF_ERR(status)
	{
		return status;
	}

	struct kprocess *proc = kmalloc(sizeof(struct kprocess));
	assert(proc);
	uprocess_init(proc);

	struct kthread *thrd = kmalloc(sizeof(struct kthread));
	assert(thrd);
	kthread_init(thrd, path, priority, proc, 1);

	// Allocate kernel stack
	vaddr_t stack_addr = (vaddr_t)vm_kalloc(KSTACK_SIZE, 0);
	stack_addr += KSTACK_SIZE;

	thrd->kstack_top = (void *)stack_addr;

	uintptr_t entry = elf_load(vn, proc);

	// allocate stack afterwards, as proc may be static and not relocatable
	vaddr_t user_stack_addr;
	EXPECT(vm_map(&proc->map, NULL, KiB(128), 0, 0, VM_RW | VM_USER,
		      VM_INHERIT_SHARED, VM_CACHE_DEFAULT, &user_stack_addr));

	user_stack_addr += KiB(128);

	kthread_context_init(thrd, thrd->kstack_top, kernel_enter_userspace,
			     (void *)entry, (void *)user_stack_addr);

	sched_resume(thrd);

	return YAK_SUCCESS;
}
