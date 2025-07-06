#include <stdint.h>
#include <string.h>

#include "gdt.h"

struct [[gnu::packed]] gdt_entry {
	uint16_t limit;
	uint16_t low;
	uint8_t mid;
	uint8_t access;
	uint8_t granularity;
	uint8_t high;
};

struct [[gnu::packed]] gdt_tss_entry {
	uint16_t length;
	uint16_t low;
	uint8_t mid;
	uint8_t access;
	uint8_t flags;
	uint8_t high;
	uint32_t upper;
	uint32_t rsv;
};

struct [[gnu::packed]] gdt {
	struct gdt_entry entries[5];
	struct gdt_tss_entry tss;
};

static struct gdt gdt;

static void gdt_make_entry(int index, uint32_t base, uint16_t limit,
			   uint8_t access, uint8_t granularity)
{
	struct gdt_entry *ent = &gdt.entries[index];
	ent->limit = limit;
	ent->access = access;
	ent->granularity = granularity;
	ent->low = (uint16_t)base;
	ent->mid = (uint8_t)(base >> 16);
	ent->high = (uint8_t)(base >> 24);
}

static void gdt_tss_entry()
{
	gdt.tss.length = 104;
	gdt.tss.access = 0x89;
	gdt.tss.flags = 0;
	gdt.tss.rsv = 0;

	gdt.tss.low = 0;
	gdt.tss.mid = 0;
	gdt.tss.high = 0;
	gdt.tss.upper = 0;
}

void gdt_init()
{
	memset(&gdt, 0, sizeof(struct gdt));
	gdt_make_entry(GDT_NULL, 0, 0, 0, 0);
	gdt_make_entry(GDT_KERNEL_CODE, 0, 0, 0x9a, 0x20);
	gdt_make_entry(GDT_KERNEL_DATA, 0, 0, 0x92, 0);
	gdt_make_entry(GDT_USER_DATA, 0, 0, 0xf2, 0);
	gdt_make_entry(GDT_USER_CODE, 0, 0, 0xfa, 0x20);
	gdt_tss_entry();
}

void gdt_reload()
{
	struct [[gnu::packed]] {
		uint16_t limit;
		uint64_t base;
	} gdtr = {
		.limit = sizeof(struct gdt) - 1,
		.base = (uint64_t)&gdt,
	};

	asm volatile( //
		"lgdt (%0)\n\t"
		// reload CS
		"pushq %1\n\t"
		"leaq 1f(%%rip), %%rax\n\t"
		"pushq %%rax\n\t"
		"lretq\n\t"
		"1:\n\t"
		// reload DS
		"mov %2, %%ds\n\t"
		"mov %2, %%es\n\t"
		"mov %2, %%ss\n\t"
		"xor %%eax, %%eax\n\t"
		"mov %%ax, %%fs\n\t"
		//
		:
		: "r"(&gdtr), "i"((uint16_t)GDT_SEL_KERNEL_CODE),
		  "r"(GDT_SEL_KERNEL_DATA)
		: "rax", "memory"
		//
	);
}
