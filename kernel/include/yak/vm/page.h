#pragma once

#include <stddef.h>
#include <yak/list.h>
#include <yak/arch-mm.h>
#include <yak/vm.h>
#include <yak/types.h>

struct page {
	paddr_t pfn;
	size_t shares;
	unsigned int max_order;
	struct list_head list_entry;
};

static inline paddr_t page_to_addr(struct page *page)
{
	return (page->pfn << PAGE_SHIFT);
}

static inline vaddr_t page_to_mapped_addr(struct page *page)
{
	return p2v(page_to_addr(page));
}

void page_zero(struct page *page, unsigned int order);
