#include <stdint.h>
#include <stddef.h>
#include <yak/cpudata.h>
#include <yak/log.h>
#include <yak/io/console.h>
#include <yak/vm/map.h>
#include <yak/vm/pmm.h>
#include <uacpi/event.h>
#include <uacpi/uacpi.h>

#include "asm.h"

#define COM1 0x3F8

uint8_t serial_buf[64];
int pos = 0;

static inline void serial_putc(uint8_t c)
{
	if (c == '\n')
		serial_putc('\r');
	while (!(inb(COM1 + 5) & 0x20))
		asm volatile("pause");
	outb(COM1, c);
}

static inline void serial_flush()
{
	for (int i = 0; i < pos; i++) {
		serial_putc(serial_buf[i]);
	}
	pos = 0;
}

size_t serial_write([[maybe_unused]] struct console *console, const char *str,
		    size_t len)
{
	for (size_t i = 0; i < len; i++) {
		serial_buf[pos++] = str[i];
		if (str[i] == '\n' || pos == sizeof(serial_buf))
			serial_flush();
	}
	return len;
}

struct console com1_console = {
	.name = "COM1",
	.write = serial_write,
};

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

	console_register(&com1_console);
}

void apic_global_init();
void lapic_enable();

void plat_sched_available()
{
	void *buf = (void *)p2v(pmm_alloc_zeroed());
	uacpi_setup_early_table_access(buf, PAGE_SIZE);

	extern status_t hpet_setup();
	EXPECT(hpet_setup());

	apic_global_init();
	lapic_enable();

	uacpi_status uret = uacpi_initialize(0);
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_initialize error: %s\n",
			 uacpi_status_to_string(uret));
		return;
	}

	uret = uacpi_namespace_load();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_namespace_load error: %s\n",
			 uacpi_status_to_string(uret));
		return;
	}

	uret = uacpi_namespace_initialize();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_namespace_initialize: %s\n",
			 uacpi_status_to_string(uret));
		return;
	}

	uret = uacpi_finalize_gpe_initialization();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_finalize_gpe_initialization: %s\n",
			 uacpi_status_to_string(uret));
		return;
	}
}
