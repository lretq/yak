#include <stdint.h>
#include <assert.h>
#include <yak/cpudata.h>
#include <yak/panic.h>
#include <yak/status.h>
#include <yak/vm/pmap.h>
#include <yak/vm/map.h>

#include "asm.h"

enum {
	LAPIC_REG_ID = 0x20,
	LAPIC_REG_VERSION = 0x30,
	LAPIC_REG_TPR = 0x80,
	LAPIC_REG_APR = 0x90,
	LAPIC_REG_PPR = 0xA0,
	LAPIC_REG_EOI = 0xB0,
	LAPIC_REG_RRD = 0xC0,
	LAPIC_REG_LOGICAL_DEST = 0xD0,
	LAPIC_REG_DEST_FORMAT = 0xE0,
	LAPIC_REG_SPURIOUS = 0xF0,
	LAPIC_REG_ISR = 0x100,
	LAPIC_REG_TMR = 0x180,
	LAPIC_REG_IRR = 0x200,
	LAPIC_REG_STATUS = 0x280,
	LAPIC_REG_CMCI = 0x2F0,
	LAPIC_REG_ICR0 = 0x300,
	LAPIC_REG_ICR1 = 0x310,
	LAPIC_REG_LVT_TIMER = 0x320,
	LAPIC_REG_LVT_THERMAL = 0x330,
	LAPIC_REG_LVT_PERFMON = 0x340,
	LAPIC_REG_LVT_LINT0 = 0x350,
	LAPIC_REG_LVT_LINT1 = 0x360,
	LAPIC_REG_LVT_ERROR = 0x370,

	LAPIC_REG_TIMER_INITIAL = 0x380,
	LAPIC_REG_TIMER_CURRENT = 0x390,
	LAPIC_REG_TIMER_DIVIDE = 0x3E0,
};

static uintptr_t apic_vbase;

static uintptr_t read_phys_base()
{
	return rdmsr(MSR_LAPIC_BASE) & 0xffffffffff000;
}

static void lapic_write(uint16_t offset, uint32_t value)
{
	assert((offset & 15) == 0);
	asm volatile("movl %1, (%0)" ::"r"((void *)(apic_vbase + offset)),
		     "r"(value)
		     : "memory");
}

static uint32_t lapic_read(uint16_t offset)
{
	uint32_t val;
	assert((offset & 15) == 0);

	asm volatile("movl (%1), %0"
		     : "=r"(val)
		     : "r"((void *)(apic_vbase + offset))
		     : "memory");

	return val;
}

uint8_t lapic_id()
{
	return lapic_read(LAPIC_REG_ID) >> 24;
}

uint8_t lapic_version()
{
	return lapic_read(LAPIC_REG_VERSION) & 0xFF;
}

void lapic_eoi()
{
	lapic_write(LAPIC_REG_EOI, 0);
}

void lapic_send_ipi(uint32_t lapic_id, uint8_t vector)
{
	lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
	lapic_write(LAPIC_REG_ICR0, vector);
}

static void lapic_mask(uint16_t offset)
{
	lapic_write(offset, (1 << 16));
}

void lapic_enable()
{
	lapic_mask(LAPIC_REG_LVT_TIMER);
	lapic_mask(LAPIC_REG_LVT_ERROR);
	lapic_mask(LAPIC_REG_LVT_LINT0);
	lapic_mask(LAPIC_REG_LVT_LINT1);
	lapic_mask(LAPIC_REG_LVT_PERFMON);
	lapic_mask(LAPIC_REG_LVT_THERMAL);

	lapic_eoi();

	// send spurious interrupts to 0xFF, enable apic
	lapic_write(LAPIC_REG_SPURIOUS, (1 << 8) | 0xFF);

	lapic_write(LAPIC_REG_TIMER_INITIAL, 0);
	lapic_write(LAPIC_REG_TIMER_CURRENT, 0);

	curcpu().md.apic_id = lapic_id();

	lapic_eoi();
}

void apic_global_init()
{
	EXPECT(vm_map_mmio(kmap(), read_phys_base(), PAGE_SIZE,
			   VM_RW | VM_GLOBAL, VM_UC, &apic_vbase));
}
