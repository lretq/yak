#pragma once

#include <stdint.h>
#include <stddef.h>
#include <yak/list.h>
#include <yak/arch-mm.h>
#include <yak/vm.h>

struct page {
	uint64_t pfn;
	size_t shares;
	unsigned int max_order;
	struct list_head list_entry;
};

static inline uintptr_t page_to_addr(struct page *page)
{
	return (page->pfn << PAGE_SHIFT);
}

static inline uintptr_t page_to_mapped_addr(struct page *page)
{
	return p2v(page_to_addr(page));
}
