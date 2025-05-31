#include <stddef.h>
#include <limine.h>
#include <yak/panic.h>
#include <yak/log.h>
#include <yak/vm/pmm.h>

#define LIMINE_REQ [[gnu::used, gnu::section(".limine_requests")]]

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
}

// first thing called after boot
void plat_boot();

// kernel entrypoint for every limine-based arch
void _start()
{
	plat_boot();

	limine_mem_init();

	panic("end of init reached\n");
}
