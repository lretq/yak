/*
https://web.cs.ucdavis.edu/~roper/ecs150/ULE.pdf

From my rough understanding we have three classes:
- idle
- time-shared
- real-time

We have three different queues: idle, current, next

idle queue is only ran once we don't have any thread whatsover on any other queue
time-shared threads may not preempt any other thread
real-time threads may preempt lower priority threads

Threads may be deemed interactive and end up on current queues too / handled as realtime threads

i'll implement everything very primitively. interactivity and other features will follow when userspace :^)

The idea for the scheduler mechanisms are from MINTIA (by @hyenasky)
See: https://github.com/xrarch/mintia2
*/

#define pr_fmt(fmt) "sched: " fmt

#include <assert.h>
#include <string.h>
#include <yak/log.h>
#include <yak/sched.h>
#include <yak/softint.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>
#include <yak/queue.h>
#include <yak/macro.h>
#include <yak/heap.h>
#include <yak/vm/pmm.h>
#include <yak/arch-cpudata.h>
#include <yak/timer.h>

struct kprocess kproc0;

void sched_init()
{
}

static void wait_for_switch(struct kthread *thread)
{
	while (__atomic_load_n(&thread->switching, __ATOMIC_ACQUIRE)) {
		busyloop_hint();
	}
}

extern void asm_swtch(struct kthread *current, struct kthread *new);

// called after switching off stack is complete
void sched_finalize_swtch(struct kthread *thread)
{
	spinlock_unlock_noipl(&thread->thread_lock);
	__atomic_store_n(&thread->switching, 0, __ATOMIC_RELEASE);
}

static void swtch(struct kthread *current, struct kthread *thread)
{
	assert(curipl() == IPL_DPC);
	assert(current && thread);
	assert(current != thread);
	assert(spinlock_held(&current->thread_lock));
	assert(!spinlock_held(&thread->thread_lock));

	current->affinity_cpu = curcpu_ptr();

	curcpu().current_thread = thread;
	curcpu().kstack_top = thread->kstack_top;

	asm_swtch(current, thread);

	// we should be back now
	assert(current == curthread());
}

void sched_preempt(struct cpu *cpu)
{
	spinlock_lock_noipl(&cpu->sched_lock);
	struct kthread *next = cpu->next_thread;
	if (!next)
		return;
	cpu->next_thread = NULL;
	next->status = THREAD_SWITCHING;
	spinlock_unlock_noipl(&cpu->sched_lock);

	struct kthread *current = cpu->current_thread;

	wait_for_switch(next);

	spinlock_lock_noipl(&current->thread_lock);

	// in the process of switching off-stack
	__atomic_store_n(&current->switching, 1, __ATOMIC_RELAXED);

	if (current != &cpu->idle_thread) {
		spinlock_lock_noipl(&cpu->sched_lock);
		sched_insert(cpu, current, 0);
		spinlock_unlock_noipl(&cpu->sched_lock);
	} else {
		// the idle thread remains ready
		current->status = THREAD_READY;
	}

	swtch(current, next);
}

static struct kthread *select_next(struct cpu *cpu, unsigned int priority)
{
	assert(spinlock_held(&cpu->sched_lock));
	struct sched *sched = &cpu->sched;

	// threads on current runqueue
	if (sched->current_rq->mask) {
		unsigned int next_priority =
			31 - __builtin_clz(sched->current_rq->mask);
		if (priority > next_priority)
			return NULL;

		struct thread_list *rq =
			&sched->current_rq->queue[next_priority];

		struct kthread *thread = TAILQ_FIRST(rq);
		assert(thread);
		TAILQ_REMOVE(rq, thread, thread_entry);
		// if now empty, update mask
		if (TAILQ_EMPTY(rq)) {
			sched->current_rq->mask &= ~(1UL << next_priority);
		}
		return thread;
	} else if (sched->next_rq->mask) {
		pr_warn("swap next and current\n");
		// NOTE: should I check priority here too?
		struct runqueue *tmp = sched->current_rq;
		sched->current_rq = sched->next_rq;
		sched->next_rq = tmp;

		// safe: can only recurse once
		return select_next(cpu, priority);
	} else if (!TAILQ_EMPTY(&sched->idle_rq)) {
		// only run idle priority if no other threads are ready
		struct kthread *thread = TAILQ_FIRST(&sched->idle_rq);
		TAILQ_REMOVE(&sched->idle_rq, thread, thread_entry);
		return thread;
	}

	return NULL;
}

#if 0
// call this from preemption handler
static void do_reschedule()
{
	struct kthread *current = curthread(), *next;
	ipl_t ipl = spinlock_lock(&curcpu_ptr()->sched_lock);
	// check if there is something ready to preempt us
	if ((next = select_next(curcpu_ptr(), current->priority)) != NULL) {
		assert(curcpu_ptr()->next_thread == NULL);
		curcpu_ptr()->next_thread = next;
	}
	spinlock_unlock(&curcpu_ptr()->sched_lock, ipl);
}
#endif

void kprocess_init(struct kprocess *process)
{
	spinlock_init(&process->process_lock);
	LIST_INIT(&process->thread_list);
	process->thread_count = 0;
}

void kthread_init(struct kthread *thread, const char *name,
		  unsigned int initial_priority, struct kprocess *process)
{
	spinlock_init(&thread->thread_lock);

	thread->switching = 0;

	strncpy(thread->name, name, sizeof(thread->name) - 1);
	thread->name[sizeof(thread->name) - 1] = '\0';

	thread->kstack_top = NULL;

	thread->wait_blocks = NULL;

	thread->timeout_wait_block.status = YAK_TIMEOUT;
	thread->timeout_wait_block.thread = thread;
	thread->timeout_wait_block.object = &thread->timeout_timer;
	timer_init(&thread->timeout_timer);

	thread->priority = initial_priority;
	thread->status = THREAD_UNDEFINED;

	thread->affinity_cpu = NULL;
	thread->last_cpu = NULL;

	ipl_t ipl = spinlock_lock(&process->process_lock);
	__atomic_fetch_add(&process->thread_count, 1, __ATOMIC_ACQUIRE);

	LIST_INSERT_HEAD(&process->thread_list, thread, process_entry);

	spinlock_unlock(&process->process_lock, ipl);

	thread->parent_process = process;
}

[[gnu::no_instrument_function]]
void sched_yield(struct kthread *current, struct cpu *cpu)
{
	assert(spinlock_held(&current->thread_lock));
	spinlock_lock_noipl(&cpu->sched_lock);
	// anything goes now
	struct kthread *next = select_next(cpu, 0);
	spinlock_unlock_noipl(&cpu->sched_lock);

	if (next) {
		wait_for_switch(next);
		swtch(current, next);
	} else {
		swtch(current, &cpu->idle_thread);
	}
}

void sched_insert(struct cpu *cpu, struct kthread *thread, int isOther)
{
	// TODO: update thread stats maybe?
	thread->last_cpu = cpu;
	thread->status = THREAD_READY;

	struct sched *sched = &cpu->sched;
	assert(spinlock_held(&cpu->sched_lock));

	struct kthread *current = cpu->current_thread, *next = cpu->next_thread;
	struct kthread *comp = next ? next : current;

	// TODO: check interactivity ??
	if (thread->priority >= SCHED_PRIO_REAL_TIME) {
		if (thread->priority <= comp->priority) {
			struct thread_list *list =
				&sched->current_rq->queue[thread->priority];
			// can't preempt now, so insert it into current runqueue
			TAILQ_INSERT_TAIL(list, thread, thread_entry);
			assert(TAILQ_FIRST(list) != NULL);
			sched->current_rq->mask |= (1UL << thread->priority);
			return;
		}
	} else {
		if (comp != &cpu->idle_thread) {
			if (thread->priority == SCHED_PRIO_IDLE) {
				TAILQ_INSERT_TAIL(&sched->idle_rq, thread,
						  thread_entry);
			} else {
				TAILQ_INSERT_TAIL(
					&sched->next_rq->queue[thread->priority],
					thread, thread_entry);
				sched->next_rq->mask |=
					(1UL << thread->priority);
			}

			// these cannot preempt, never
			return;
		}
	}

	// can preempt currently running thread
	thread->status = THREAD_NEXT;

	cpu->next_thread = thread;

	if (next) {
		// reinsert old next thread if we preempted
		// next prio < our prio
		// TODO: should I hold next thread's lock o?
		sched_insert(cpu, next, isOther);
	}

	if (likely(!isOther)) {
		softint_issue(IPL_DPC);
	} else {
		softint_issue_other(cpu, IPL_DPC);
	}
}

struct cpu *find_cpu()
{
	// TODO: check cpu loads
	return curcpu_ptr();
}

void sched_resume_locked(struct kthread *thread)
{
	assert(spinlock_held(&thread->thread_lock));

	struct cpu *cpu = thread->affinity_cpu;
	if (!cpu) {
		cpu = find_cpu();
	}

	spinlock_lock_noipl(&cpu->sched_lock);
	sched_insert(cpu, thread, cpu != curcpu_ptr());
	spinlock_unlock_noipl(&cpu->sched_lock);
}

void sched_resume(struct kthread *thread)
{
	ipl_t ipl = spinlock_lock(&thread->thread_lock);
	sched_resume_locked(thread);
	spinlock_unlock(&thread->thread_lock, ipl);
}

[[gnu::noreturn]]
void sched_exit_self()
{
	struct kthread *thread = curthread();
	spinlock_lock(&thread->thread_lock);

	thread->status = THREAD_TERMINATING;

	sched_yield(thread, curcpu_ptr());
	__builtin_unreachable();
	__builtin_trap();
}

status_t kernel_thread_create(const char *name, unsigned int priority,
			      void *entry, void *context, int instant_launch,
			      struct kthread **out)
{
	struct kthread *thread = kmalloc(sizeof(struct kthread));
	if (!thread)
		return YAK_OOM;

	kthread_init(thread, name, priority, &kproc0);

	pr_warn("rewrite thread stack creation!\n");

	struct page *stack_pages = pmm_alloc_order(2);
	if (!stack_pages) {
		kfree(thread, sizeof(struct kthread));
		return YAK_OOM;
	}

	void *stack_top =
		(void *)(page_to_mapped_addr(stack_pages) + PAGE_SIZE * 4);

	kthread_context_init(thread, stack_top, entry, context, NULL);

	if (out != NULL)
		*out = thread;

	if (instant_launch)
		sched_resume(thread);

	return YAK_SUCCESS;
}

void idle_loop()
{
	setipl(IPL_PASSIVE);
	while (1) {
		asm volatile("sti; hlt");
	}
}
