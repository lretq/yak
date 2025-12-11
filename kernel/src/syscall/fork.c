#include <string.h>
#include <assert.h>
#include <yak/syscall.h>
#include <yak/heap.h>
#include <yak/process.h>
#include <yak/cpudata.h>
#include <yak/queue.h>
#include <yak/log.h>
#include <yak/vm/map.h>

void _syscall_fork_return();

DEFINE_SYSCALL(SYS_FORK, fork)
{
	pr_extra_debug("sys_fork()\n");
	struct kprocess *cur_proc = curproc();
	struct kthread *cur_thread = curthread();
	struct kprocess *new_proc = kmalloc(sizeof(struct kprocess));
	assert(new_proc);
	uprocess_init(new_proc, cur_proc);

	vm_map_fork(cur_proc->map, new_proc->map);

	ipl_t ipl = spinlock_lock(&cur_proc->jobctl_lock);
	new_proc->session.leader = cur_proc->session.leader;
	struct kprocess *pgrp_lead = cur_proc->pgrp_leader;
	if (!pgrp_lead)
		pgrp_lead = cur_proc;
	new_proc->pgrp_leader = pgrp_lead;
	TAILQ_INSERT_TAIL(&pgrp_lead->pgrp_members, new_proc, pgrp_entry);
	spinlock_unlock(&cur_proc->jobctl_lock, ipl);

	{
		guard(mutex)(&cur_proc->fd_mutex);

		EXPECT(fd_grow(new_proc, cur_proc->fd_cap));

		for (int i = 0; i < cur_proc->fd_cap; i++) {
			struct fd *desc = fd_safe_get(cur_proc, i);
			if (!desc)
				continue;
			struct fd *desc_copy = kmalloc(sizeof(struct fd));
			desc_copy->file = desc->file;
			desc_copy->flags = desc->flags;
			file_ref(desc->file);
			new_proc->fds[i] = desc_copy;
			assert(fd_safe_get(new_proc, i));
		}
	}

	struct kthread *new_thread = kmalloc(sizeof(struct kthread));
	assert(new_thread);
	kthread_init(new_thread, cur_thread->name, cur_thread->priority,
		     new_proc, 1);
	kthread_context_copy(cur_thread, new_thread);

	vaddr_t stack_addr = (vaddr_t)vm_kalloc(KSTACK_SIZE, 0);
	stack_addr += KSTACK_SIZE;
	new_thread->kstack_top = (void *)stack_addr;

	stack_addr -= sizeof(struct syscall_frame);
	assert((stack_addr & 0xF) == 0);

	struct syscall_frame *new_frame = (void *)stack_addr;
	memcpy(new_frame, __syscall_ctx, sizeof(struct syscall_frame));

	// child gets pid 0
	new_frame->rax = 0;
	// and also no error
	new_frame->rdx = 0;

	uint64_t *sp = (uint64_t *)stack_addr;
	sp -= 2;
	*sp = (uint64_t)_syscall_fork_return;

	new_thread->pcb.rsp = (uint64_t)sp;

	sched_resume(new_thread);

	return SYS_OK(new_proc->pid);
}
