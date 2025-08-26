#include <stdint.h>
#include <yak/log.h>
#include <yak/arch-cpu.h>
#include <yak/ipl.h>
#include <yak/irq.h>
#include <yak/vm.h>
#include <yak/vm/map.h>
#include <yak/cpudata.h>

#include "gdt.h"
#include "apic.h"
#include "asm.h"

struct [[gnu::packed]] idt_entry {
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t ist;
	uint8_t attributes;
	uint16_t isr_mid;
	uint32_t isr_high;
	uint32_t rsv;
};

static void idt_make_entry(struct idt_entry *entry, uint64_t isr)
{
	entry->isr_low = (uint16_t)(isr);
	entry->isr_mid = (uint16_t)(isr >> 16);
	entry->isr_high = (uint32_t)(isr >> 32);
	entry->kernel_cs = GDT_SEL_KERNEL_CODE;
	entry->rsv = 0;
	entry->ist = 0;
	entry->attributes = 0x8E;
}

static void idt_set_ist(struct idt_entry *entry, uint8_t ist)
{
	entry->ist = ist;
}

[[gnu::aligned(16)]]
static struct idt_entry idt[256];
extern uint64_t itable[256];
irq_handler *ihandlers[IRQ_SLOTS];
extern struct irq_slot slots[IRQ_SLOTS];

void default_handler([[maybe_unused]] void *frame, irq_vec_t vector)
{
	pr_warn("received unhandled irq (%d)\n", vector);
}

void plat_set_irq_handler(irq_vec_t vec, irq_handler *fn)
{
	assert(vec >= 0 && vec < IRQ_SLOTS);
	if (fn == NULL)
		fn = default_handler;
	ihandlers[vec] = fn;
}

void idt_init()
{
	for (int i = 0; i < 256; i++) {
		idt_make_entry(&idt[i], itable[i]);
	}

	for (int i = 0; i < IRQ_SLOTS; i++) {
		ihandlers[i] = default_handler;
	}

	// nmi
	idt_set_ist(&idt[2], 1);
	// double fault
	idt_set_ist(&idt[8], 2);
	// debug / breakpoint
	idt_set_ist(&idt[1], 3);
	idt_set_ist(&idt[3], 3);
}

void idt_reload()
{
	struct [[gnu::packed]] {
		uint16_t limit;
		uint64_t base;
	} idtr = { .limit = sizeof(struct idt_entry) * 256 - 1,
		   .base = (uint64_t)&idt[0] };

	asm volatile("lidt %0" ::"m"(idtr) : "memory");
}

struct [[gnu::packed]] context {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rbp;

	uint64_t number;
	uint64_t error;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

static status_t handle_pf(uintptr_t address, uint64_t error)
{
	// TODO: check error for read, write, ... for handler flags
	(void)error;

	unsigned long flags = 0;

	if ((address & 0xff00000000000000) == 0)
		flags |= VM_FAULT_USER;

	return vm_handle_fault(kmap(), address, flags);
}

void __isr_c_entry(struct context *frame)
{
	if (frame->number >= 0 && frame->number <= 31) {
		if (frame->number == 14) {
			status_t status;
			IF_OK((status = handle_pf(read_cr2(), frame->error)))
			{
				return;
			}
			pr_error("#PF handling failed with status '%s'\n",
				 status_str(status));
			pr_error("cr2=0x%lx at address 0x%lx\n", frame->error,
				 read_cr2());
		} else {
			pr_error("fault 0x%lx received\n", frame->number);
		}
		pr_error("fault at ip 0x%lx\n", frame->rip);
		hcf();
	} else {
		ipl_t ipl = ripl(frame->number >> 4);
		irq_vec_t vec = frame->number - 32;
		struct irq_slot *sl = &slots[vec];

		// TODO: replace with masking
		if (sl->pinconf.trigger != PIN_TRG_LEVEL) {
			lapic_eoi();
		}

		ihandlers[vec](frame, vec);

		if (sl->pinconf.trigger == PIN_TRG_LEVEL) {
			lapic_eoi();
		}

		xipl(ipl);
	}
}
