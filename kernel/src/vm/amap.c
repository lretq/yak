#include "yak/refcount.h"
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/vm/object.h>
#include <yak/vm/pmm.h>
#include <yak/vm/page.h>
#include <yak/vm/amap.h>

static void anon_free(struct vm_anon *anon)
{
	if (anon->page) {
		page_deref(anon->page);
		anon->page = NULL;
	}

	kfree(anon, sizeof(struct vm_anon));
}

GENERATE_REFMAINT_INLINE(vm_anon, refcnt, anon_free);

struct vm_amap *vm_amap_create(struct vm_object *obj)
{
	struct vm_amap *amap = kmalloc(sizeof(struct vm_amap));
	amap->refcnt = 1;
	amap->l3 = NULL;
	amap->obj = obj;
	vm_object_ref(obj);
	return amap;
}

static void amap_deref_all(struct vm_amap *amap)
{
	for (size_t i = 0; i < elementsof(amap->l3->entries); i++) {
		struct vm_amap_l2 *l2e = amap->l3->entries[i];
		if (!l2e)
			continue;

		for (size_t j = 0; j < elementsof(l2e->entries); j++) {
			struct vm_amap_l1 *l1e = l2e->entries[j];
			if (!l1e)
				continue;
			for (size_t k = 0; k < elementsof(l1e->entries); k++) {
				struct vm_anon *anon = l1e->entries[k];
				if (!anon)
					continue;

				vm_anon_deref(anon);

				l1e->entries[k] = NULL;
			}

			kfree(l1e, sizeof(struct vm_amap_l1));
		}

		kfree(l2e, sizeof(struct vm_amap_l2));
	}
}

static void amap_cleanup(struct vm_amap *amap)
{
	// drop references to anons, pages, and the backing object

	if (amap->l3)
		amap_deref_all(amap);

	vm_object_deref(amap->obj);

	kfree(amap, sizeof(struct vm_amap));
}

GENERATE_REFMAINT(vm_amap, refcnt, amap_cleanup);

struct vm_anon **vm_amap_lookup(struct vm_amap *amap, voff_t offset, int create)
{
	assert(amap);

	size_t l3, l2, l1;
	// mimic a 3level page table
	size_t pg = offset >> 12;
	l3 = (pg >> 18) & 511;
	l2 = (pg >> 9) & 511;
	l1 = pg & 511;

	if (!amap->l3) {
		if (!create)
			return NULL;

		amap->l3 = kmalloc(sizeof(struct vm_amap_l3));
		memset(amap->l3, 0, sizeof(struct vm_amap_l3));
	}

	struct vm_amap_l2 *l2_entry = amap->l3->entries[l3];
	if (!l2_entry) {
		if (!create)
			return NULL;

		l2_entry = kmalloc(sizeof(struct vm_amap_l2));
		memset(l2_entry, 0, sizeof(struct vm_amap_l2));
		amap->l3->entries[l3] = l2_entry;
	}

	struct vm_amap_l1 *l1_entry = l2_entry->entries[l2];
	if (!l1_entry) {
		if (!create)
			return NULL;

		l1_entry = kmalloc(sizeof(struct vm_amap_l1));
		memset(l1_entry, 0, sizeof(struct vm_amap_l1));
		l2_entry->entries[l2] = l1_entry;
	}

	return &l1_entry->entries[l1];
}
