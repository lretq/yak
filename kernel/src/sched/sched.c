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
#include <yak/cpu.h>
#include <yak/kevent.h>
#include <yak/dpc.h>
#include <yak/sched.h>
#include <yak/softint.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>
#include <yak/queue.h>
#include <yak/macro.h>
#include <yak/heap.h>
#include <yak/vm/pmm.h>
#include <yak/vm/map.h>
#include <yak/arch-cpudata.h>
#include <yak/timer.h>

struct kprocess kproc0;

static struct kevent reaper_ev;
static SPINLOCK(reaper_lock);
static struct thread_list reaper_queue = TAILQ_HEAD_INITIALIZER(reaper_queue);
static struct kthread *reaper_thread;

void sched_init()
{
	spinlock_init(&reaper_lock);
	TAILQ_INIT(&reaper_queue);
	event_init(&reaper_ev, 0);
}

[[gnu::no_instrument_function, gnu::always_inline]]
static void wait_for_switch(struct kthread *thread)
{
	while (__atomic_load_n(&thread->switching, __ATOMIC_ACQUIRE)) {
		busyloop_hint();
	}
}

[[gnu::no_instrument_function]]
void plat_swtch(struct kthread *current, struct kthread *new);

// called after switching off stack is complete
[[gnu::no_instrument_function]]
void sched_finalize_swtch(struct kthread *thread)
{
	spinlock_unlock_noipl(&thread->thread_lock);
	__atomic_store_n(&thread->switching, 0, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]]
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

	if (unlikely(thread->vm_ctx != NULL)) {
		assert(thread->parent_process == &kproc0);
		vm_map_activate(thread->vm_ctx);
	} else if (thread->user_thread) {
		if (current->parent_process != thread->parent_process)
			vm_map_activate(&thread->parent_process->map);
	}

	plat_swtch(current, thread);

	assert(current->status != THREAD_TERMINATING);

	// we should be back now
	assert(current == curthread());
}

[[gnu::no_instrument_function]]
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
		thread->status = THREAD_SWITCHING;
		return thread;
	} else if (sched->next_rq->mask) {
#if 0
		pr_warn("swap next and current\n");
#endif
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
		thread->status = THREAD_SWITCHING;
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

static uint64_t next_pid = 0;

void kprocess_init(struct kprocess *process)
{
	process->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
	spinlock_init(&process->process_lock);
	LIST_INIT(&process->thread_list);
	process->thread_count = 0;

	if (likely(process != &kproc0)) {
		vm_map_init(&process->map);
	}
}

void kthread_init(struct kthread *thread, const char *name,
		  unsigned int initial_priority, struct kprocess *process,
		  int user_thread)
{
	spinlock_init(&thread->thread_lock);

	thread->switching = 0;

	strncpy(thread->name, name, sizeof(thread->name) - 1);
	thread->name[sizeof(thread->name) - 1] = '\0';

	thread->kstack_top = NULL;

	thread->user_thread = user_thread;

	thread->wait_blocks = NULL;

	thread->timeout_wait_block.status = YAK_TIMEOUT;
	thread->timeout_wait_block.thread = thread;
	thread->timeout_wait_block.object = &thread->timeout_timer;
	timer_init(&thread->timeout_timer);

	thread->priority = initial_priority;
	thread->status = THREAD_UNDEFINED;

	thread->affinity_cpu = NULL;
	thread->last_cpu = NULL;

	thread->vm_ctx = NULL;

	ipl_t ipl = spinlock_lock(&process->process_lock);
	__atomic_fetch_add(&process->thread_count, 1, __ATOMIC_ACQUIRE);

	LIST_INSERT_HEAD(&process->thread_list, thread, process_entry);

	spinlock_unlock(&process->process_lock, ipl);

	thread->parent_process = process;
}

[[gnu::no_instrument_function]]
void sched_yield(struct kthread *current, struct cpu *cpu)
{
	assert(current);
	assert(cpu);
	assert(spinlock_held(&current->thread_lock));
	spinlock_lock_noipl(&cpu->sched_lock);
	// anything goes now
	struct kthread *next = cpu->next_thread;
	if (next) {
		cpu->next_thread = NULL;
		next->status = THREAD_SWITCHING;
	} else {
		next = select_next(cpu, 0);
	}

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

static size_t count = 0;

struct cpu *find_cpu()
{
	assert(cpus_online() != 0);

	size_t cpu, i = 0,
		    desired = __atomic_fetch_add(&count, 1, __ATOMIC_ACQUIRE) %
			      cpus_online();

	for_each_cpu(cpu, &cpumask_active) {
		if (i++ == desired)
			return getcpu(cpu);
	}

	pr_warn("find_cpu(): fallback to local core?\n");

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

void thread_reaper_fn()
{
	for (;;) {
		sched_wait_single(&reaper_ev, WAIT_MODE_BLOCK, WAIT_TYPE_ANY,
				  TIMEOUT_INFINITE);

		ipl_t ipl = spinlock_lock(&reaper_lock);

		struct kthread *thread;

		while (!TAILQ_EMPTY(&reaper_queue)) {
			thread = TAILQ_FIRST(&reaper_queue);
			TAILQ_REMOVE(&reaper_queue, thread, thread_entry);

			spinlock_unlock(&reaper_lock, ipl);

			kthread_destroy(thread);

			ipl = spinlock_lock(&reaper_lock);
		}

		spinlock_unlock(&reaper_lock, ipl);
	}
}

void sched_dynamic_init()
{
	kernel_thread_create("reaper_thread", SCHED_PRIO_REAL_TIME_END,
			     thread_reaper_fn, NULL, 1, &reaper_thread);
}

[[gnu::noreturn]]
void sched_exit_self()
{
	ripl(IPL_DPC);

	struct kthread *thread = curthread();

	spinlock_lock_noipl(&thread->thread_lock);

	thread->status = THREAD_TERMINATING;

	struct cpu *cpu = curcpu_ptr();

	spinlock_lock_noipl(&reaper_lock);
	TAILQ_INSERT_HEAD(&reaper_queue, thread, thread_entry);
	event_alarm(&reaper_ev);
	spinlock_unlock_noipl(&reaper_lock);

	sched_yield(thread, cpu);
	__builtin_unreachable();
	__builtin_trap();
}

void kthread_destroy(struct kthread *thread)
{
	assert(thread->status == THREAD_TERMINATING);

	struct kprocess *process = thread->parent_process;

	ipl_t ipl = spinlock_lock(&process->process_lock);
	if (0 ==
	    __atomic_sub_fetch(&process->thread_count, 1, __ATOMIC_ACQUIRE)) {
		pr_warn("no thread left for process (implement destroying processes)\n");
	}

	LIST_REMOVE(thread, process_entry);

	spinlock_unlock(&process->process_lock, ipl);

	vm_unmap(kmap(), (vaddr_t)thread->kstack_top - KSTACK_SIZE);

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

void idle_loop()
{
	setipl(IPL_PASSIVE);
	while (1) {
		assert(curipl() == IPL_PASSIVE);
		asm volatile("sti; hlt");
	}
}
