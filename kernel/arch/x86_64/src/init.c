#include <stdint.h>
#include <stddef.h>
#include <yak/cpudata.h>
#include <yak/log.h>
#include <yak/vm/map.h>
#include "asm.h"

#define COM1 0x3F8

static inline void serial_putc(uint8_t c)
{
	if (c == '\n')
		serial_putc('\r');
	while (!(inb(COM1 + 5) & 0x20))
		asm volatile("pause");
	outb(COM1, c);
}

void serial_puts(const char *str, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		serial_putc(str[i]);
	}
}

void idt_init();
void idt_reload();
void gdt_init();
void gdt_reload();

[[gnu::section(".percpu.cpudata"), gnu::used]]
struct cpu percpu_cpudata = {};

void plat_boot()
{
	// we can just use the "normal" variables on the BSP for percpu access
	wrmsr(MSR_GSBASE, 0);

	// can pass the start of the .percpu.cpudata offset for the later inits
	cpudata_init(&percpu_cpudata);

	idt_init();
	idt_reload();
	gdt_init();
	gdt_reload();
}
