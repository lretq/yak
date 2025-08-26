#include <stdint.h>
#include <yak/vm.h>
#include <yak/vm/map.h>
#include <yak/percpu.h>
#include <yak/cpudata.h>
#include <string.h>

#include "gdt.h"
#include "tss.h"

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
	struct gdt_entry entries[GDT_TSS];
	struct gdt_tss_entry tss;
};

__percpu struct gdt t_gdt;
__percpu struct tss t_tss;

static void gdt_make_entry(int index, uint32_t base, uint16_t limit,
			   uint8_t access, uint8_t granularity)
{
	t_gdt.entries[index].limit = limit;
	t_gdt.entries[index].access = access;
	t_gdt.entries[index].granularity = granularity;
	t_gdt.entries[index].low = (uint16_t)base;
	t_gdt.entries[index].mid = (uint8_t)(base >> 16);
	t_gdt.entries[index].high = (uint8_t)(base >> 24);
}

static void gdt_tss_entry()
{
	t_gdt.tss.length = 104;
	t_gdt.tss.access = 0x89;
	t_gdt.tss.flags = 0;
	t_gdt.tss.rsv = 0;

	t_gdt.tss.low = 0;
	t_gdt.tss.mid = 0;
	t_gdt.tss.high = 0;
	t_gdt.tss.upper = 0;
}

void gdt_init()
{
	gdt_make_entry(GDT_NULL, 0, 0, 0, 0);
	gdt_make_entry(GDT_KERNEL_CODE, 0, 0, 0x9a, 0x20);
	gdt_make_entry(GDT_KERNEL_DATA, 0, 0, 0x92, 0);
	gdt_make_entry(GDT_USER_DATA, 0, 0, 0xf2, 0);
	gdt_make_entry(GDT_USER_CODE, 0, 0, 0xfa, 0x20);
	gdt_tss_entry();
}

static vaddr_t alloc_kstack()
{
	vaddr_t stack_addr;
	EXPECT(vm_map(kmap(), NULL, KSTACK_SIZE, 0, 0, VM_RW | VM_PREFILL,
		      VM_INHERIT_NONE, &stack_addr));
	return stack_addr + KSTACK_SIZE;
}

static void tss_reload()
{
	uint64_t tss_addr = (uint64_t)PERCPU_PTR(void, t_tss);
	t_gdt.tss.low = (uint16_t)tss_addr;
	t_gdt.tss.mid = (uint8_t)(tss_addr >> 16);
	t_gdt.tss.high = (uint8_t)(tss_addr >> 24);
	t_gdt.tss.upper = (uint32_t)(tss_addr >> 32);

	t_gdt.tss.flags = 0;
	t_gdt.tss.access = 0b10001001;
	t_gdt.tss.rsv = 0;
	asm volatile("ltr %0" ::"rm"(GDT_SEL_TSS) : "memory");
}

void tss_set_rsp0(uint64_t stack_top)
{
	t_tss.rsp0 = stack_top;
}

void tss_init()
{
	memset(PERCPU_PTR(void, t_tss), 0, sizeof(struct tss));
	t_tss.ist1 = alloc_kstack();
	t_tss.ist2 = alloc_kstack();
	t_tss.ist3 = alloc_kstack();
	t_tss.ist4 = alloc_kstack();
	tss_reload();
}

void gdt_reload()
{
	struct [[gnu::packed]] {
		uint16_t limit;
		uint64_t base;
	} gdtr = {
		.limit = sizeof(struct gdt) - 1,
		.base = (uint64_t)PERCPU_PTR(struct gdt, t_gdt),
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
