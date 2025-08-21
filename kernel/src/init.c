#define pr_fmt(fmt) "init: " fmt

#include <yak/sched.h>
#include <yak/log.h>
#include <yak/heap.h>
#include <yak/irq.h>
#include <yak/cpu.h>
#include <yak/vm/map.h>
#include <string.h>
#include <yak/fs/vfs.h>

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

extern status_t vfs_create(char *path, enum vtype type);

extern void tmpfs_init();

status_t vfs_lookup_path(const char *path, struct vnode *cwd, int flags,
			 struct vnode **out, char **last_comp);

void kmain()
{
	pr_info("enter kmain()\n");

	// start other cores
	extern void plat_start_aps();
	plat_start_aps();

	// if setup, displays system information
	extern void kinfo_launch();
	kinfo_launch();

	// init io subsystem
	extern void io_init();
	io_init();

	// threads will be reaped now
	sched_dynamic_init();

	// TODO: init VFS, add a vfs hook for initramfs, then load /bin/init

	vfs_init();
	tmpfs_init();

	EXPECT(vfs_mount("/", "tmpfs"));
	EXPECT(vfs_create("/var", VDIR));
	EXPECT(vfs_mount("/var/", "tmpfs"));
	EXPECT(vfs_create("/var/tmp", VDIR));
	EXPECT(vfs_mount("/var/tmp", "tmpfs"));

	struct vnode *out;
	//EXPECT(vfs_lookup_path("/", NULL, 0, &out, NULL));
	//EXPECT(vfs_lookup_path("/tmp", NULL, 0, &out, NULL));

	//EXPECT(vfs_mount("/tmp", "tmpfs"));

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

	kernel_thread_create("kmain", SCHED_PRIO_REAL_TIME, kmain, NULL, 1,
			     NULL);

	// our stack is cpu0's idle stack
	extern void idle_loop();
	idle_loop();
}
