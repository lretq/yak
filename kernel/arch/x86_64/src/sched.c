#include <stdint.h>
#include <yak/sched.h>
#include <yak/log.h>
#include "gdt.h"

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

[[gnu::noreturn]]
void kernel_enter_userspace(uint64_t ip, uint64_t sp)
{
	pr_debug("enter userspace: 0x%lx rsp: 0x%lx\n", ip, sp);

	uint64_t frame[5];
	frame[0] = ip;
	frame[1] = GDT_SEL_USER_CODE;
	frame[2] = 0x200;
	frame[3] = sp;
	frame[4] = GDT_SEL_USER_DATA;

	asm volatile("mov %0, %%rsp\n\t"
		     "swapgs\n\t"
		     "iretq\n\t" ::"r"(frame)
		     : "memory");

	__builtin_unreachable();
}
