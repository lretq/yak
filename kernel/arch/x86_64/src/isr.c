#include <stdint.h>
#include <yak/log.h>

#include "gdt.h"

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

void idt_init()
{
	for (int i = 0; i < 256; i++) {
		idt_make_entry(&idt[i], itable[i]);
	}

	// nmi
	idt_set_ist(&idt[2], 1);
	// debug / breakpoint
	idt_set_ist(&idt[1], 2);
	idt_set_ist(&idt[3], 2);
	// double fault
	idt_set_ist(&idt[8], 3);
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

void __isr_c_entry(void *frame)
{
	(void)frame;
	printk(LOG_WARN, "received int\n");
}
