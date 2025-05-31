#pragma once

#include <stdint.h>
#include <yak/vm/page.h>

enum {
	ZONE_1MB = 0,
	ZONE_LOW,
	ZONE_HIGH,
};

void pmm_init();

void pmm_zone_init(int zone_id, const char *name, int may_alloc, uintptr_t base,
		   uintptr_t end);

void pmm_add_region(uintptr_t base, uintptr_t end);

struct page *pmm_lookup_page(uintptr_t addr);

struct page *pmm_dma_alloc_order(unsigned int order);
#define pmm_dma_free_order(pa, order) pmm_free_order(pa, order)

struct page *pmm_alloc_order(unsigned int order);

void pmm_free_pages_order(struct page *page, unsigned int order);

void pmm_free_order(uintptr_t addr, unsigned int order);

void pmm_dump();

static inline uintptr_t pmm_alloc()
{
	return page_to_addr(pmm_alloc_order(0));
}

static inline void pmm_free(uintptr_t pa)
{
	pmm_free_order(pa, 0);
}
