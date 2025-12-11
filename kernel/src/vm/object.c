#include <assert.h>
#include <yak/cleanup.h>
#include <yak/mutex.h>
#include <yak/types.h>
#include <yak/tree.h>
#include <yak/vm/map.h>
#include <yak/vm/page.h>
#include <yak/vm/object.h>

void vm_object_common_init(struct vm_object *obj, struct vm_pagerops *pgops)
{
	kmutex_init(&obj->obj_lock, "vm_object");
	RBT_INIT(vm_page_tree, &obj->memq);
	obj->pg_ops = pgops;
	obj->refcnt = 1;
}

#define LOOKUP_ONLY 0x1
status_t vm_lookuppage(struct vm_object *obj, voff_t offset, int flags,
		       struct page **pagep)
{
	guard(mutex)(&obj->obj_lock);

	struct page key = (struct page){ .offset = offset };
	struct page *pg = RBT_FIND(vm_page_tree, &obj->memq, &key);

	if (pg) {
		*pagep = pg;
		return YAK_SUCCESS;
	}

	if (flags & LOOKUP_ONLY) {
		*pagep = NULL;
		return YAK_NOENT;
	}

	unsigned int npages = 1;
	status_t res = obj->pg_ops->pgo_get(obj, offset, &pg, &npages, 0, VM_RW,
					    flags);
	if (IS_ERR(res)) {
		return res;
	}

	RBT_INSERT(vm_page_tree, &obj->memq, pg);

	*pagep = pg;

	return YAK_SUCCESS;
}

static void vm_object_cleanup(struct vm_object *obj)
{
	assert(obj->pg_ops->pgo_cleanup);
	obj->pg_ops->pgo_cleanup(obj);
}

GENERATE_REFMAINT(vm_object, refcnt, vm_object_cleanup);
