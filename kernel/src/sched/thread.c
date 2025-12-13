#include <yak/sched.h>
#include <yak/log.h>
#include <yak/heap.h>

void kthread_destroy(struct kthread *thread)
{
	assert(thread->status == THREAD_TERMINATING);

	struct kprocess *process = thread->owner_process;

	ipl_t ipl = spinlock_lock(&process->thread_list_lock);
	if (0 ==
	    __atomic_sub_fetch(&process->thread_count, 1, __ATOMIC_ACQUIRE)) {
		pr_warn("no thread left for process (implement destroying processes)\n");
		if (process->pid == 1)
			panic("attempted to kill init!\n");
	}

	LIST_REMOVE(thread, process_entry);

	spinlock_unlock(&process->thread_list_lock, ipl);

	vaddr_t stack_base = (vaddr_t)thread->kstack_top - KSTACK_SIZE;
	vm_kfree((void *)stack_base, KSTACK_SIZE);

	kfree(thread, sizeof(struct kthread));
}

status_t kernel_thread_create(const char *name, unsigned int priority,
			      void *entry, void *context, int instant_launch,
			      struct kthread **out)
{
	struct kthread *thread = kmalloc(sizeof(struct kthread));
	if (!thread)
		return YAK_OOM;

	kthread_init(thread, name, priority, &kproc0, 0);

	vaddr_t stack_addr = (vaddr_t)vm_kalloc(KSTACK_SIZE, 0);

	if (stack_addr == 0) {
		kfree(thread, sizeof(struct kthread));
		return YAK_OOM;
	}

	void *stack_top = (void *)(stack_addr + KSTACK_SIZE);

	kthread_context_init(thread, stack_top, entry, context, NULL);

	if (out != NULL)
		*out = thread;

	if (instant_launch)
		sched_resume(thread);

	return YAK_SUCCESS;
}
