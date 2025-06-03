#include <yak/panic.h>
#include <yak/list.h>
#include <yak/dpc.h>
#include <yak/ipl.h>
#include <yak/cpudata.h>

#define PENDING(ipl) (1UL << ((ipl) - 1))

static void softint_ack(struct cpu *cpu, ipl_t ipl)
{
	__atomic_and_fetch(&cpu->softint_pending, ~PENDING(ipl),
			   __ATOMIC_RELEASE);
}

static void do_dpc_int(struct cpu *cpu)
{
	softint_ack(cpu, IPL_DPC);
	setipl(IPL_DPC);

	enable_interrups();

	dpc_queue_run(cpu);

	if (cpu->next_thread) {
		panic("handle thread switch");
	}

	disable_interrupts();
}

static void (*softint_handlers[IPL_HIGH])(struct cpu *) = {
	NULL, /* later: APC */
	do_dpc_int,
};

void softint_dispatch(ipl_t ipl)
{
	int state = disable_interrupts();

	struct cpu *cpu = curcpu_ptr();

	while (cpu->softint_pending & (0xFF << ipl)) {
		softint_handlers[31 - __builtin_clz(cpu->softint_pending)](cpu);
		cpu = curcpu_ptr();
	}

	setipl(ipl);

	if (state)
		enable_interrups();
}

void softint_issue(ipl_t ipl)
{
	__atomic_or_fetch(&curcpu_ptr()->softint_pending, PENDING(ipl),
			  __ATOMIC_ACQUIRE);
}

void softint_issue_other(struct cpu *cpu, ipl_t ipl)
{
	__atomic_or_fetch(&cpu->softint_pending, PENDING(ipl),
			  __ATOMIC_ACQUIRE);
	panic("ipi?");
}
