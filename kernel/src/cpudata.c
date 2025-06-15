#include <stddef.h>
#include <nanoprintf.h>
#include <yak/queue.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>

static size_t cpu_id = 0;

extern void timer_update(struct dpc *dpc, void *ctx);

void cpudata_init(struct cpu *cpu, void *stack_top)
{
	cpu->self = cpu;

	cpu->cpu_id = __atomic_fetch_add(&cpu_id, 1, __ATOMIC_RELAXED);

	cpu->kstack_top = NULL;
	cpu->next_thread = NULL;
	cpu->current_thread = NULL;

	cpu->softint_pending = 0;

	spinlock_init(&cpu->dpc_lock);
	LIST_INIT(&cpu->dpc_queue);

	HEAP_INIT(&cpu->timer_heap);
	dpc_init(&cpu->timer_update_dpc, timer_update);

	cpu->kstack_top = stack_top;
	cpu->idle_thread.kstack_top = stack_top;

	curcpu().current_thread = &curcpu_ptr()->idle_thread;

	char idle_name[12];
	npf_snprintf(idle_name, sizeof(idle_name), "idle%ld", cpu->cpu_id);
	kthread_init(&curcpu_ptr()->idle_thread, idle_name, 0, &kproc0);
}
