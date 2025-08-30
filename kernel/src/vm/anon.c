#include <stddef.h>
#include <assert.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/tree.h>
#include <yak/vm/map.h>
#include <yak/vm/pmm.h>
#include <yak/vm/page.h>
#include <yak/vm/object.h>

struct vm_aobj {
	struct vm_object obj;
};

status_t anon_pager_get(struct vm_object *obj, voff_t offset,
			struct page **pages, unsigned int *npages,
			[[maybe_unused]] unsigned int centeridx,
			[[maybe_unused]] vm_prot_t access_type,
			[[maybe_unused]] unsigned int flags)
{
	assert(IS_ALIGNED_POW2(offset, PAGE_SIZE));
	struct vm_aobj *aobj = (struct vm_aobj *)obj;

	unsigned int i;
	for (i = 0; i < *npages; i++) {
		// TODO: if page had a swap slot, we should swap it in!
		// this is all a big TODO, as we don't support swap yet
		pages[i] = vm_pagealloc(obj, offset);
		page_zero(pages[i], 0);
	}

	return YAK_SUCCESS;
}

uintptr_t anon_pager_put(struct vm_object *object, struct page **pages,
			 voff_t offset)
{
	// TODO: swap out
	return 0;
}

void anon_pager_cleanup(struct vm_object *object)
{
	struct page *elm;
	RBT_FOREACH(elm, vm_page_tree, &object->memq)
	{
		vm_page_release(elm);
	}

	kfree(object, sizeof(struct vm_aobj));
}

void anon_pager_ref(struct vm_object *object)
{
	__atomic_fetch_add(&object->refcnt, 1, __ATOMIC_SEQ_CST);
}

struct vm_pagerops anon_pagerops = {
	.pgo_name = "anon",
	.pgo_init = NULL,
	.pgo_get = anon_pager_get,
	.pa_put = anon_pager_put,
	.pgo_ref = anon_pager_ref,
	.pgo_cleanup = anon_pager_cleanup,
};

struct vm_object *vm_aobj_create()
{
	struct vm_aobj *aobj = kmalloc(sizeof(struct vm_aobj));
	vm_object_common_init(&aobj->obj, &anon_pagerops);
	return &aobj->obj;
}
