#define pr_fmt(fmt) "init: " fmt

#include <yak/sched.h>
#include <yak/log.h>
#include <yak/heap.h>
#include <yak/irq.h>
#include <yak/cpu.h>
#include <yak/vm/map.h>

#include <config.h>

#define GET_ASCII_ART
#include "art.h"
#undef GET_ASCII_ART

void plat_boot();
void plat_mem_init();

[[gnu::weak]]
void plat_heap_available()
{
}

[[gnu::weak]]
void plat_irq_available()
{
}

typedef void (*func_ptr)(void);
extern func_ptr __init_array[];
extern func_ptr __init_array_end[];

void kmain()
{
	pr_info("enter kmain()\n");
	sched_dynamic_init();
	pr_info("dynamic init\n");

	// TODO: init VFS, add a vfs hook for initramfs, then load /bin/init

	for (int i = 0; i < 100; i++) {
		vaddr_t addr;
		vm_map(kmap(), NULL, PAGE_SIZE, 0, 0, VM_RW, VM_INHERIT_NONE,
		       &addr);

		char *buf = (char *)addr;
		*buf = '\0';
		void *memset(void *, int, size_t);
		memset(buf, 0x69, PAGE_SIZE);
		vm_unmap(kmap(), addr);
	}

#if 1
	// if setup, displays system information
	extern void kinfo_launch();
	kinfo_launch();
#endif

#if 0
	extern void PerformFireworksTest();
	kernel_thread_create("fwtst", SCHED_PRIO_REAL_TIME,
			     PerformFireworksTest, NULL, 1, NULL);
#endif

	ksleep(STIME(10000));

	sched_exit_self();
}

void kstart()
{
	kprocess_init(&kproc0);

	// expected to:
	// * init cpudata
	// * set idle thread stack
	plat_boot();

	sched_init();

	cpu_init();
	// declare BSP as up and running
	cpu_up(0);

	for (func_ptr *func = __init_array; func < __init_array_end; ++func) {
		(*func)(); // Call constructor
	}

	// because why not :^)
	kputs(bootup_ascii_txt);

	pr_info("Yak-" ARCH " v" VERSION_STRING " booting\n");

	// expected to:
	// * add currently usable PMM regions
	// * setup zones
	// * init kmap
	plat_mem_init();

	// init kernel heap arena & kmalloc suite
	heap_init();
	plat_heap_available();

	irq_init();

	// e.g. x86 calibrates LAPIC
	plat_irq_available();

	// init io subsystem
	extern void io_init();
	io_init();

	extern void plat_start_aps();
	plat_start_aps();

	kernel_thread_create("kmain", SCHED_PRIO_REAL_TIME, kmain, NULL, 1,
			     NULL);

	// our stack is cpu0's idle stack
	extern void idle_loop();
	idle_loop();
}
