#include "yak/log.h"
#include <assert.h>
#include <yak/spinlock.h>
#include <yak/arch-cpudata.h>
#include <yak/ipl.h>
#include <yak/list.h>
#include <yak/dpc.h>
#include <yak/softint.h>
#include <yak/cpudata.h>

void dpc_init(struct dpc *dpc, void (*func)(struct dpc *, void *))
{
	dpc->enqueued = 0;
	dpc->func = func;
	dpc->context = NULL;
	list_init(&dpc->dpc_list);
}

void dpc_enqueue(struct dpc *dpc, void *context)
{
	struct cpu *cpu = curcpu_ptr();
	int state = spinlock_lock_interrupts(&cpu->dpc_lock);

	if (dpc->enqueued) {
		spinlock_unlock_interrupts(&cpu->dpc_lock, state);
		return;
	}

	dpc->enqueued = 1;
	dpc->context = context;
	list_add(&cpu->dpc_queue, &dpc->dpc_list);

	softint_issue(IPL_DPC);
	spinlock_unlock_interrupts(&cpu->dpc_lock, state);

	ipl_t ipl = curipl();
	if (ipl < IPL_DPC) {
		softint_dispatch(ipl);
	}
}

void dpc_dequeue(struct dpc *dpc)
{
	struct cpu *cpu = curcpu_ptr();
	int state = spinlock_lock_interrupts(&cpu->dpc_lock);

	list_del(&dpc->dpc_list);
	dpc->enqueued = 0;

	spinlock_unlock_interrupts(&cpu->dpc_lock, state);
}

void dpc_queue_run(struct cpu *cpu)
{
	assert(curipl() == IPL_DPC);

	void *context;
	struct dpc *dpc;
	while (1) {
		int state = spinlock_lock_interrupts(&cpu->dpc_lock);
		if (list_empty(&cpu->dpc_queue)) {
			spinlock_unlock_interrupts(&cpu->dpc_lock, state);
			return;
		}

		dpc = list_entry(list_pop_front(&cpu->dpc_queue), struct dpc,
				 dpc_list);
		assert(dpc);
		assert(dpc->func);

		context = dpc->context;
		dpc->enqueued = 0;

		spinlock_unlock_interrupts(&cpu->dpc_lock, state);
		dpc->func(dpc, context);
	}
}
