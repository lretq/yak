#define pr_fmt(fmt) "init: " fmt

#include <yak/init.h>
#include <yak/sched.h>
#include <yak/log.h>
#include <yak/process.h>
#include <yak/jobctl.h>
#include <yak/heap.h>
#include <yak/irq.h>
#include <yak/cpu.h>
#include <yak/tty.h>
#include <yak/io/console.h>
#include <yak/vm/map.h>
#include <yak/fs/vfs.h>

#include <config.h>

#define GET_ASCII_ART
#include "art.h"
#undef GET_ASCII_ART

void init_bsp_cpudata();

typedef void (*func_ptr)(void);
extern func_ptr __init_array[];
extern func_ptr __init_array_end[];

INIT_STAGE(bsp_ready);
INIT_STAGE(heap_ready);
INIT_STAGE(aps_ready);

void kmain()
{
	pr_info("enter kmain() thread\n");

	INIT_RUN_STAGE(aps_ready);

	init_run_all();

	tty_init();
	console_init();

	struct kprocess *proc1 = kzalloc(sizeof(struct kprocess));
	uprocess_init(proc1, &kproc0);

	jobctl_setsid(proc1);

	assert(proc1->pid == 1);
	char *init_args[] = { "/sbin/init", NULL };
	char *init_envp[] = { NULL };
	struct kthread *init_thrd;
	EXPECT(launch_elf(proc1, "/sbin/init", SCHED_PRIO_TIME_SHARE, init_args,
			  init_envp, &init_thrd));
	sched_resume(init_thrd);

#if 0
	extern void PerformFireworksTest();
	kernel_thread_create("fwtst", SCHED_PRIO_REAL_TIME,
			     PerformFireworksTest, NULL, 1, NULL);
#endif

	sched_exit_self();
}

void kstart()
{
	kprocess_init(&kproc0);

	// expected to:
	// * init bsp cpudata
	// * setup basic cpu environment for the bsp
	// * possibly setup early output sink
	init_bsp_cpudata();

	sched_init();

	cpu_init();
	// declare BSP as up and running
	cpu_up(0);

	for (func_ptr *func = __init_array; func < __init_array_end; ++func) {
		(*func)(); // call constructors
	}

	// show the yak
	kputs(bootup_ascii_txt);

	pr_info("Yak-" ARCH_STR " v" VERSION_STRING " booting\n");

	// setup the kernel init machine
	init_setup();

	// expected state from platform:
	// * remap kernel
	// * arch pmm zones setup
	// * added all physical RAM regions
	//
	// expected state from kernel:
	// * vmem/kernel VA
	// * kernel heap
	INIT_RUN_STAGE(bsp_ready);

	kernel_thread_create("kmain", SCHED_PRIO_REAL_TIME, kmain, NULL, 1,
			     NULL);

	// our stack is cpu0's idle stack
	extern void idle_loop();
	idle_loop();
}
