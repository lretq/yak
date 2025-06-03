#include <stdint.h>
#include <yak/sched.h>

extern void asm_thread_trampoline();

void kthread_context_init(struct kthread *thread, void *kstack_top,
			  void *entrypoint, void *context1, void *context2)
{
	thread->kstack_top = kstack_top;

	uint64_t *sp = (uint64_t *)kstack_top;
	sp -= 2;
	*sp = (uint64_t)asm_thread_trampoline;

	thread->pcb.rsp = (uint64_t)sp;
	thread->pcb.r12 = (uint64_t)entrypoint;
	thread->pcb.r13 = (uint64_t)context1;
	thread->pcb.r14 = (uint64_t)context2;
}
