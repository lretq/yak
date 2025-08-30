#include <stdint.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/log.h>

#include "gdt.h"
#include "tss.h"
#include "asm.h"

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

	thread->pcb.fsbase = 0;
	thread->pcb.gsbase = 0;
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
		     "xor %%rax, %%rax\n\t"
		     "xor %%rbx, %%rbx\n\t"
		     "xor %%rcx, %%rcx\n\t"
		     "xor %%rdx, %%rdx\n\t"
		     "xor %%rsi, %%rsi\n\t"
		     "xor %%rdi, %%rdi\n\t"
		     "xor %%rbp, %%rbp\n\t"
		     "xor %%r8, %%r8\n\t"
		     "xor %%r9, %%r9\n\t"
		     "xor %%r10, %%r10\n\t"
		     "xor %%r11, %%r11\n\t"
		     "xor %%r12, %%r12\n\t"
		     "xor %%r13, %%r13\n\t"
		     "xor %%r14, %%r14\n\t"
		     "xor %%r15, %%r15\n\t"
		     "swapgs\n\t"
		     "iretq\n\t" ::"r"(frame)
		     : "memory");

	__builtin_unreachable();
}

[[gnu::no_instrument_function]]
extern void asm_swtch(struct kthread *current, struct kthread *new);

__no_prof __no_san void plat_swtch(struct kthread *current,
				   struct kthread *thread)
{
	if (thread->user_thread) {
		t_tss.rsp0 = (uint64_t)thread->kstack_top;
		wrmsr(MSR_FSBASE, thread->pcb.fsbase);
		wrmsr(MSR_KERNEL_GSBASE, thread->pcb.gsbase);
	} else {
		wrmsr(MSR_FSBASE, 0);
	}

	asm_swtch(current, thread);
}
