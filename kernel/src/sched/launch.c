#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <yak/vm/map.h>
#include <yak/fs/vfs.h>
#include <yak/sched.h>
#include <yak/elf_common.h>
#include <yak/log.h>
#include <yak/status.h>
#include <yak/elf.h>
#include <yak/macro.h>
#include <yak/heap.h>

#define INTERP_BASE 0x7ffff7dd7000

struct load_info {
	uintptr_t real_entry;
	uintptr_t prog_entry;
	uintptr_t base;
	uintptr_t phdr;
	size_t phnum;
};

static status_t elf_load_at(char *path, struct kprocess *process,
			    struct load_info *loadinfo, uintptr_t base);

static status_t elf_load(struct vnode *vn, struct kprocess *process,
			 struct load_info *loadinfo, uintptr_t base)
{
	Elf64_Ehdr ehdr;
	size_t read = -1;
	EXPECT(vfs_read(vn, 0, &ehdr, sizeof(Elf64_Ehdr), &read));

	bool pie = (ehdr.e_type == ET_DYN);

	Elf64_Phdr *phdrs = kmalloc(ehdr.e_phnum * ehdr.e_phentsize);
	guard(autofree)(phdrs, ehdr.e_phnum * ehdr.e_phentsize);

	for (size_t idx = 0; idx < ehdr.e_phnum; idx++) {
		size_t phoff = ehdr.e_phoff + idx * ehdr.e_phentsize;
		EXPECT(vfs_read(vn, phoff, &phdrs[idx], ehdr.e_phentsize,
				&read));
	}

	vaddr_t min_va = UINTPTR_MAX;
	vaddr_t max_va = 0;

	for (size_t i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr *phdr = &phdrs[i];
		if (phdr->p_type != PT_LOAD)
			continue;

		vaddr_t seg_start = ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
		vaddr_t seg_end =
			ALIGN_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);

		min_va = MIN(seg_start, min_va);
		max_va = MAX(seg_end, max_va);
	}

	if (base == 0 && pie)
		base = PAGE_SIZE;

	size_t reserved_size = max_va - min_va;

	status_t res = vm_map_reserve(&process->map, base, reserved_size,
				      pie ? VM_MAP_FIXED | VM_MAP_OVERWRITE : 0,
				      &base);
	IF_ERR(res) return res;

	pr_debug("reserve: %lx\n", base);

	// correct the load bias
	size_t load_bias = pie ? base - min_va : 0;

	pr_info("ehdr.e_phnum: %u\n", ehdr.e_phnum);
	for (size_t i = 0; i < ehdr.e_phnum; i++) {
		Elf64_Phdr *phdr = &phdrs[i];
		if (phdr->p_type != PT_LOAD)
			continue;

		if (phdr->p_filesz > phdr->p_memsz) {
			pr_warn("the file size cannot be larger than the mem size.\n");
			return YAK_INVALID_ARGS;
		}

		// Align addresses
		vaddr_t va_base =
			ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE) + load_bias;
		voff_t page_off =
			phdr->p_vaddr - ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
		size_t map_size = ALIGN_UP(page_off + phdr->p_memsz, PAGE_SIZE);

		/* nothing to map */
		if (map_size == 0)
			break;

		vaddr_t seg_addr = 0;
		res = vm_map(&process->map, NULL, map_size, 0,
			     VM_RW | VM_USER | VM_EXECUTE, VM_INHERIT_SHARED,
			     VM_CACHE_DEFAULT, va_base,
			     VM_MAP_FIXED | VM_MAP_OVERWRITE, &seg_addr);
		IF_ERR(res) return res;

		pr_extra_debug("PT_LOAD: %lx - %lx\n", seg_addr,
			       seg_addr + map_size);

		EXPECT(vfs_read(vn, phdr->p_offset,
				(void *)(seg_addr + page_off), phdr->p_filesz,
				&read));

		// zero bss
		memset((void *)(seg_addr + page_off + phdr->p_filesz), 0,
		       phdr->p_memsz - phdr->p_filesz);
	}

	loadinfo->prog_entry = ehdr.e_entry + load_bias;
	loadinfo->real_entry = loadinfo->prog_entry;
	loadinfo->base = base;
	loadinfo->phnum = ehdr.e_phnum;

	pr_extra_debug("load_bias : %lx\n", load_bias);

	for (size_t idx = 0; idx < ehdr.e_phnum; idx++) {
		Elf64_Phdr *phdr = &phdrs[idx];
		switch (phdr->p_type) {
		case PT_INTERP: {
			size_t interp_len = phdr->p_filesz;
			char *interp = kmalloc(interp_len);

			EXPECT(vfs_read(vn, phdr->p_offset, interp, interp_len,
					&read));

			pr_extra_debug("PT_INTERP: %s\n", interp);

			struct load_info interpinfo;
			EXPECT(elf_load_at(interp, process, &interpinfo,
					   INTERP_BASE));

			loadinfo->base = interpinfo.base;
			loadinfo->real_entry = interpinfo.prog_entry;

			kfree(interp, interp_len);
			break;
		}
		case PT_PHDR:
			loadinfo->phdr = phdr->p_vaddr + load_bias;
			break;
		/* no-op, rtld needs this */
		case PT_DYNAMIC:
			/* no-op */
		case PT_LOAD:
		case PT_NOTE:
		case PT_NULL:
			break;
		default:
			/* some gnu bs */
			pr_extra_debug("unhandled phdr type: %u\n",
				       phdr->p_type);
		}
	}

	return YAK_SUCCESS;
}

static status_t elf_load_at(char *path, struct kprocess *process,
			    struct load_info *info, uintptr_t base)
{
	struct vnode *vn;
	status_t status = vfs_open(path, &vn);
	IF_ERR(status)
	{
		return status;
	}
	return elf_load(vn, process, info, base);
}

struct auxv_pair {
	uint64_t type;
	uint64_t value;
};
_Static_assert(sizeof(struct auxv_pair) == 16);

static size_t setup_auxv(struct auxv_pair *auxv, struct load_info *info)
{
	size_t i = 0;
	auxv[i++] = (struct auxv_pair){ AT_PHDR, info->phdr };
	auxv[i++] = (struct auxv_pair){ AT_PHENT, sizeof(Elf64_Phdr) };
	auxv[i++] = (struct auxv_pair){ AT_PHNUM, info->phnum };
	auxv[i++] = (struct auxv_pair){ AT_ENTRY, info->prog_entry };
	auxv[i++] = (struct auxv_pair){ AT_PAGESZ, PAGE_SIZE };
	auxv[i++] =
		(struct auxv_pair){ AT_BASE,
				    info->base }; // ld.so base if PIE/interp
	auxv[i++] = (struct auxv_pair){ AT_UID, 0 };
	auxv[i++] = (struct auxv_pair){ AT_EUID, 0 };
	auxv[i++] = (struct auxv_pair){ AT_GID, 0 };
	auxv[i++] = (struct auxv_pair){ AT_EGID, 0 };
	return i;
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

	vm_map_tmp_switch(&proc->map);
	struct load_info info;
	EXPECT(elf_load(vn, proc, &info, 0));

	pr_debug("our real entry is: %lx\n", info.real_entry);
	pr_debug("our prog entry is: %lx\n", info.prog_entry);

	// allocate stack afterwards, as proc may be static and not relocatable
	vaddr_t user_stack_addr;
	EXPECT(vm_map(&proc->map, NULL, USER_STACK_LENGTH, 0, VM_RW | VM_USER,
		      VM_INHERIT_COPY, VM_CACHE_DEFAULT, USER_STACK_BASE, 0,
		      &user_stack_addr));
	user_stack_addr += USER_STACK_LENGTH;

	user_stack_addr = ALIGN_DOWN(user_stack_addr, 16);

	const char *argv_strings[] = { path, NULL };
	size_t argc = 0;
	for (size_t i = 0; argv_strings[i]; i++) {
		argc++;
	}

	//const char *envp_strings[] = { "LD_SHOW_AUXV=1", NULL };
	const char *envp_strings[] = { NULL };
	size_t envc = 0;
	for (size_t i = 0; envp_strings[i]; i++) {
		envc++;
	}

	char **argv_ptr = kmalloc(argc * sizeof(char *));
	char **envp_ptr = kmalloc(envc * sizeof(char *));

	for (size_t i = 0; i < argc; i++) {
		size_t len = strlen(argv_strings[i]) + 1;
		user_stack_addr -= len;
		memcpy((void *)user_stack_addr, argv_strings[i], len);
		argv_ptr[i] = (void *)user_stack_addr;
	}

	for (size_t i = 0; i < envc; i++) {
		size_t len = strlen(envp_strings[i]) + 1;
		user_stack_addr -= len;
		memcpy((void *)user_stack_addr, envp_strings[i], len);
		envp_ptr[i] = (void *)user_stack_addr;
	}

	user_stack_addr = ALIGN_DOWN(user_stack_addr, 16);

	struct auxv_pair auxv[16];
	size_t auxvc = setup_auxv(auxv, &info);

	// plus envc words + null word
	// plus argc words + null word
	// plus one argc word for count
	// plus one null auxv
	size_t words = envc + argc + auxvc * 2 + 4;

	if (((user_stack_addr - words * sizeof(uintptr_t)) & 15) != 0) {
		user_stack_addr -= 8;
	}

	uintptr_t *sp = (uintptr_t *)user_stack_addr;

	// auxv
	*--sp = AT_NULL;
	for (size_t i = 0; i < auxvc; i++) {
		*--sp = auxv[i].value;
		*--sp = auxv[i].type;
	}

	pr_debug("auxv at %p\n", sp);

	// envp
	*--sp = 0;
	for (size_t i = 0; i < envc; i++) {
		*--sp = (uintptr_t)envp_ptr[envc - i - 1];
	}

	pr_debug("envp at %p\n", sp);

	// argv
	*--sp = 0;
	for (size_t i = 0; i < argc; i++) {
		*--sp = (uintptr_t)argv_ptr[argc - i - 1];
	}

	pr_debug("argv at %p\n", sp);

	*--sp = argc;

	pr_debug("argc at %p\n", sp);

	assert(((uintptr_t)sp & 15) == 0);

	vm_map_tmp_disable();

	kthread_context_init(thrd, thrd->kstack_top, kernel_enter_userspace,
			     (void *)info.real_entry, sp);

	if (proc->pid == 1) {
		pr_debug("setup init fds\n");

		void user_stdio_setup(struct kprocess * proc);
		user_stdio_setup(proc);
	}

	sched_resume(thrd);

	return YAK_SUCCESS;
}
