#include <stddef.h>
#include <nanoprintf.h>
#include <yak/queue.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>

struct cpumask {
	uint64_t bits[1]; // for now 64 cpus
};

struct cpumask cpumask_active;
size_t num_cpus_active = 0;

void cpu_init()
{
	__atomic_store_n(&cpumask_active.bits[0], 0, __ATOMIC_RELAXED);
}

void cpu_up(size_t id)
{
	assert(id < 64);
	__atomic_fetch_or(&cpumask_active.bits[0], (1 << id), __ATOMIC_ACQUIRE);
	__atomic_fetch_add(&num_cpus_active, 1, __ATOMIC_ACQUIRE);
}

size_t cpus_online()
{
	return __atomic_load_n(&num_cpus_active, __ATOMIC_ACQUIRE);
}

static size_t cpu_id = 0;

extern void timer_update(struct dpc *dpc, void *ctx);

static struct cpu *bsp_ptr;
struct cpu **__all_cpus = NULL;

void cpudata_init(struct cpu *cpu, void *stack_top)
{
	cpu->self = cpu;

	cpu->cpu_id = __atomic_fetch_add(&cpu_id, 1, __ATOMIC_RELAXED);
	if (cpu->cpu_id == 0) {
		bsp_ptr = cpu;
		__all_cpus = &bsp_ptr;
	}

	cpu->kstack_top = NULL;
	cpu->next_thread = NULL;
	cpu->current_thread = NULL;

	cpu->softint_pending = 0;

	spinlock_init(&curcpu_ptr()->sched_lock);
	struct sched *sched = &curcpu_ptr()->sched;

	for (size_t rq = 0; rq < 2; rq++) {
		for (size_t prio = 0; prio < SCHED_PRIO_MAX; prio++) {
			TAILQ_INIT(&sched->rqs[rq].queue[prio]);
		}
		sched->rqs[rq].mask = 0;
	}

	sched->current_rq = &sched->rqs[0];
	sched->next_rq = &sched->rqs[1];

	TAILQ_INIT(&sched->idle_rq);

	spinlock_init(&cpu->dpc_lock);
	LIST_INIT(&cpu->dpc_queue);

	spinlock_init(&cpu->timer_lock);
	HEAP_INIT(&cpu->timer_heap);
	dpc_init(&cpu->timer_update_dpc, timer_update);

	cpu->kstack_top = stack_top;
	cpu->idle_thread.kstack_top = stack_top;

	cpu->current_thread = &curcpu_ptr()->idle_thread;
	cpu->next_thread = NULL;

	char idle_name[12];
	npf_snprintf(idle_name, sizeof(idle_name), "idle%ld", cpu->cpu_id);
	kthread_init(&curcpu_ptr()->idle_thread, idle_name, 0, &kproc0);
}
