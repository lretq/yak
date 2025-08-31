#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <yak/arch-mm.h>
#include <yak/vm.h>
#include <yak/refcount.h>
#include <yak/tree.h>
#include <yak/types.h>
#include <yak/queue.h>

#define VM_PG_FAKE 0x1

struct page {
	paddr_t pfn;

	/* vm references */
	refcount_t shares;

	/* VM metadata */
	struct vm_object *vmobj; /* page owner object */
	voff_t offset; /* offset into object */

	unsigned long flags;

	/* buddy metadata */
	unsigned int order;
	unsigned int max_order;

	RBT_ENTRY(page) rb_entry;
	TAILQ_ENTRY(page) tailq_entry;
};

RBT_HEAD(vm_page_tree, page);
/* rb tree keyed by offset into object */
RBT_PROTOTYPE(vm_page_tree, page, rb_entry, page_cmp);

static inline paddr_t page_to_addr(struct page *page)
{
	return (page->pfn << PAGE_SHIFT);
}

static inline vaddr_t page_to_mapped_addr(struct page *page)
{
	return p2v(page_to_addr(page));
}

void page_zero(struct page *page, unsigned int order);

struct page *vm_pagealloc(struct vm_object *obj, voff_t offset);
void vm_pagefree(struct page *pg);

DECLARE_REFMAINT(page);

#ifdef __cplusplus
}
#endif
