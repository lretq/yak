#include <yak/heap.h>
#include <yak/tree.h>
#include <yak/types.h>
#include <yak/vm/map.h>
#include <yak/vm/pmm.h>

int vm_page_tree_cmp(const struct page *a, const struct page *b)
{
	if (a->offset < b->offset)
		return -1;
	if (a->offset > b->offset)
		return 1;
	return 0;
}

RBT_GENERATE(vm_page_tree, page, rb_entry, vm_page_tree_cmp);

void vm_pagefree(struct page *pg)
{
	pg->vmobj = NULL;
	pg->offset = 0;
	pmm_free_pages_order(pg, 0);
}

struct page *vm_pagealloc(struct vm_object *obj, voff_t offset)
{
	struct page *pg = pmm_alloc_order(0);
	if (!pg)
		return NULL;

	pg->vmobj = obj;
	pg->offset = offset;

	return pg;
}

void page_free(struct page *pg)
{
	/* page is not used by anyone anymore */
	if (pg->flags & VM_PG_FAKE) {
		// e.g. fake device page
		kfree(pg, sizeof(struct page));
	} else {
		pmm_free_pages_order(pg, pg->order);
	}
}

GENERATE_REFMAINT(page, shares, page_free);
