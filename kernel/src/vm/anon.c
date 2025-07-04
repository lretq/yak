#include "yak/heap.h"
#include "yak/vm/page.h"
#include "yak/vm/pmm.h"
#include <stddef.h>
#include <yak/vm/map.h>
#include <assert.h>
#include <yak/macro.h>

struct vm_anon **lookup(struct vm_object *obj, voff_t offset)
{
	size_t page_off = offset / PAGE_SIZE;
	panic("TODO");
}

status_t anon_pager_get(struct vm_object *obj, voff_t offset,
			struct page **pages, unsigned int *npages,
			[[maybe_unused]] unsigned int centeridx,
			[[maybe_unused]] vm_prot_t access_type,
			[[maybe_unused]] unsigned int flags)
{
	struct vm_aobj *aobj = (struct vm_aobj *)obj;

	unsigned int i;
	for (i = 0; i < *npages; i++) {
		struct vm_anon **anonp = lookup(obj, offset + i * PAGE_SIZE);
		struct vm_anon *anon = *anonp;
		if (!anon) {
			struct vm_anon *new_anon =
				kmalloc(sizeof(struct vm_anon));
			struct page *pg = pmm_alloc_order(0);
			assert(pg);
			if (!pg)
				return YAK_OOM;
			page_zero(pg, 0);

			new_anon->page = pg;
			new_anon->offset = offset + i * PAGE_SIZE;
			new_anon->refcnt = 1;

			*anonp = new_anon;
			anon = new_anon;
		}

		pages[i] = anon->page;
	}

	return YAK_SUCCESS;
}

uintptr_t anon_pager_put(struct vm_object *object, struct page **pages,
			 voff_t offset)
{
	// TODO: swap out
	return 0;
}

struct vm_pagerops anon_pagerops = {
	.pgo_name = "anon",
	.pgo_init = NULL,
	.pgo_get = anon_pager_get,
	.pa_put = anon_pager_put,
};

struct vm_amap *vm_amap_create()
{
	struct vm_amap *amap = kmalloc(sizeof(struct vm_amap));
	for (size_t i = 0; i < elementsof(amap->entries); i++) {
		amap->entries[i] = NULL;
	}
	return amap;
}
