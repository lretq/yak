#include <stddef.h>
#include <yak/queue.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>

static size_t cpu_id = 0;

void cpudata_init(struct cpu *cpu)
{
	cpu->self = cpu;

	cpu->cpu_id = __atomic_fetch_add(&cpu_id, 1, __ATOMIC_RELAXED);

	cpu->kstack_top = NULL;
	cpu->next_thread = NULL;
	cpu->current_thread = NULL;

	cpu->softint_pending = 0;

	spinlock_init(&cpu->dpc_lock);
	LIST_INIT(&cpu->dpc_queue);
}
