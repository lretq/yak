#include <stddef.h>
#include <limine.h>
#include <uacpi/uacpi.h>
#include <yak/kernel-file.h>
#include <yak/panic.h>
#include <yak/log.h>
#include <yak/object.h>
#include <yak/vm/pmm.h>
#include <yak/vm/map.h>
#include <yak/vm/pmap.h>
#include <yak/heap.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/macro.h>

#include <yak/mutex.h>

#include <config.h>

#include "request.h"

LIMINE_REQ
static volatile LIMINE_BASE_REVISION(3);

[[gnu::used, gnu::section(".limine_requests_start")]] //
static volatile LIMINE_REQUESTS_START_MARKER;

[[gnu::used, gnu::section(".limine_requests_end")]] //
static volatile LIMINE_REQUESTS_END_MARKER;

LIMINE_REQ static volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = NULL,
};

LIMINE_REQ static volatile struct limine_hhdm_request hhdm_request = {
	.id = LIMINE_HHDM_REQUEST,
	.revision = 3,
	.response = NULL,
};

LIMINE_REQ static volatile struct limine_executable_address_request
	address_request = {
		.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
		.revision = 0,
		.response = NULL,
	};

LIMINE_REQ static volatile struct limine_paging_mode_request
	paging_mode_request = {
		.id = LIMINE_PAGING_MODE_REQUEST,
		.revision = 1,
		.response = NULL,
#ifdef x86_64
		.mode = LIMINE_PAGING_MODE_X86_64_5LVL,
		.min_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
		.max_mode = LIMINE_PAGING_MODE_X86_64_5LVL,
#elif defined(riscv64)
		.mode = LIMINE_PAGING_MODE_RISCV_SV57,
		.min_mode = LIMINE_PAGING_MODE_RISCV_SV39,
		.max_mode = LIMINE_PAGING_MODE_RISCV_SV57,
#endif
	};

size_t HHDM_BASE;
size_t PMAP_LEVELS;

void limine_mem_init()
{
	struct limine_memmap_response *res = memmap_request.response;

	// setup PFNDB, HHDM and kernel mappings
	HHDM_BASE = hhdm_request.response->offset;

	switch (paging_mode_request.response->mode) {
#if defined(x86_64)
	case LIMINE_PAGING_MODE_X86_64_5LVL:
		PMAP_LEVELS = 5;
		break;
	case LIMINE_PAGING_MODE_X86_64_4LVL:
		PMAP_LEVELS = 4;
		break;
#elif defined(riscv64)
	case LIMINE_PAGING_MODE_RISCV_SV57:
		PMAP_LEVELS = 5;
		break;
	case LIMINE_PAGING_MODE_RISCV_SV48:
		PMAP_LEVELS = 4;
		break;
	case LIMINE_PAGING_MODE_RISCV_SV39:
		PMAP_LEVELS = 3;
		break;
#endif
	}

	pmm_init();

	for (size_t i = 0; i < res->entry_count; i++) {
		struct limine_memmap_entry *ent = res->entries[i];
		if (ent->type != LIMINE_MEMMAP_USABLE)
			continue;

		pmm_add_region(ent->base, ent->base + ent->length);
	}

	// bootstrap kernel pmap + setup kmap
	vm_map_init(kmap());
	struct pmap *kpmap = &kmap()->pmap;

	for (size_t i = 0; i < res->entry_count; i++) {
		struct limine_memmap_entry *ent = res->entries[i];
		if (ent->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
		    ent->type != LIMINE_MEMMAP_FRAMEBUFFER &&
		    ent->type != LIMINE_MEMMAP_EXECUTABLE_AND_MODULES &&
		    ent->type != LIMINE_MEMMAP_USABLE)
			continue;

		vm_cache_t cache = VM_CACHE_DEFAULT;
		if (ent->type == LIMINE_MEMMAP_FRAMEBUFFER)
			cache = VM_WC;

		pmap_large_map_range(kpmap, ent->base, ent->length,
				     HHDM_BASE + ent->base, VM_RW, cache);
	}

	uintptr_t kernel_pbase = address_request.response->physical_base;
	uintptr_t kernel_vbase = address_request.response->virtual_base;

#define MAP_SECTION(SECTION, VMFLAGS)                                         \
	uintptr_t SECTION##_start =                                           \
		ALIGN_DOWN((uintptr_t)__kernel_##SECTION##_start, PAGE_SIZE); \
	uintptr_t SECTION##_end =                                             \
		ALIGN_UP((uintptr_t)__kernel_##SECTION##_end, PAGE_SIZE);     \
	pmap_large_map_range(kpmap,                                           \
			     SECTION##_start - kernel_vbase + kernel_pbase,   \
			     (SECTION##_end - SECTION##_start),               \
			     SECTION##_start, (VMFLAGS), VM_CACHE_DEFAULT);

	MAP_SECTION(limine, VM_READ);
	MAP_SECTION(text, VM_RX);
	MAP_SECTION(rodata, VM_READ);
	MAP_SECTION(data, VM_RW);

#undef MAP_SECTION

	pmap_activate(kpmap);
}

// first thing called after boot
void plat_boot();

void plat_sched_available();

extern char __init_stack_top[];

struct kthread test_thread;
struct kthread test_thread2;

struct kmutex mtx;

void test_entry()
{
	kmutex_acquire(&mtx);
	pr_info("yogurt\n");
	kmutex_release(&mtx);

	sched_exit_self();
}

void vm_map_dump(struct vm_map *map);

void test2_entry()
{
	kmutex_acquire(&mtx);
	pr_info("gurt: yo\n");
	kmutex_release(&mtx);

	uintptr_t map_addr;
	uintptr_t pa_addr = pmm_alloc();
	EXPECT(vm_map_mmio(kmap(), pa_addr, PAGE_SIZE, VM_RW, VM_CACHE_DEFAULT,
			   &map_addr));

	char *m = (char *)map_addr;
	*m = '1';

	if (*m != *(char *)(p2v(pa_addr))) {
		panic("bad");
	}

	vm_unmap(kmap(), map_addr);
	pmm_free(pa_addr);

	sched_exit_self();
}

extern void limine_fb_setup();

// kernel entrypoint for every limine-based arch
void limine_start()
{
	plat_boot();

	curcpu().current_thread = &curcpu_ptr()->idle_thread;
	curcpu().kstack_top = (void *)__init_stack_top;
	curcpu().idle_thread.kstack_top = (void *)__init_stack_top;
	kprocess_init(&kproc0);
	kthread_init(&curcpu_ptr()->idle_thread, "idle0", 0, &kproc0);

	sched_init();

	pr_info("Yak-" ARCH " v" VERSION_STRING " booting\n");
	pr_info("Hai :3\n");

	limine_mem_init();

	heap_init();

	limine_fb_setup();

	kthread_init(&test_thread, "test_thread", SCHED_PRIO_REAL_TIME,
		     &kproc0);
	uintptr_t stack = p2v(pmm_alloc() + 4096);
	kthread_context_init(&test_thread, (void *)stack, test_entry, NULL,
			     NULL);

	kthread_init(&test_thread2, "test_thread2", SCHED_PRIO_REAL_TIME,
		     &kproc0);
	stack = p2v(pmm_alloc() + 4096);
	kthread_context_init(&test_thread2, (void *)stack, test2_entry, NULL,
			     NULL);

	kmutex_init(&mtx);

	ripl(IPL_DPC);

	sched_resume(&test_thread);
	pr_info("insert 1\n");
	sched_resume(&test_thread2);
	pr_info("insert 2\n");

	xipl(IPL_PASSIVE);

	plat_sched_available();

	void *buf = (void *)p2v(pmm_alloc_zeroed());
	uacpi_setup_early_table_access(buf, PAGE_SIZE);

	panic("end of init reached\n");
}

LIMINE_REQ static volatile struct limine_rsdp_request rsdp_request = {
	.id = LIMINE_RSDP_REQUEST,
	.revision = 0,
	.response = NULL
};

paddr_t plat_get_rsdp()
{
	return rsdp_request.response == NULL ? 0 :
					       rsdp_request.response->address;
}
