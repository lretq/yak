#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <yak/vm/anon.h>
#include <yak/log.h>
#include <yak/refcount.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/vm/object.h>
#include <yak/vm/pmm.h>
#include <yak/vm/page.h>
#include <yak/vm/amap.h>

struct vm_amap_l1 {
	struct vm_anon *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap_l2 {
	struct vm_amap_l1 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap_l3 {
	struct vm_amap_l2 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap *vm_amap_create(struct vm_object *obj)
{
	assert(obj != NULL);

	struct vm_amap *amap = kmalloc(sizeof(struct vm_amap));
	assert(amap != NULL);
	memset(amap, 0, sizeof(struct vm_amap));

	amap->refcnt = 1;
	amap->l3 = NULL;

	kmutex_init(&amap->lock, "amap");

	vm_object_ref(obj);
	amap->obj = obj;

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
	// at this point, every reference has been dropped
	// we now need to drop references to anons, pages, and the backing object.

	EXPECT(kmutex_acquire(&amap->lock, TIMEOUT_INFINITE));

	if (amap->l3) {
		amap_deref_all(amap);
	}

	vm_object_deref(amap->obj);

	kfree(amap, sizeof(struct vm_amap));
}

GENERATE_REFMAINT(vm_amap, refcnt, amap_cleanup);

static struct vm_anon **locked_amap_lookup(struct vm_amap *amap, voff_t offset,
					   unsigned int flags)
{
	assert(amap);

	bool create = (flags & VM_AMAP_CREATE);

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

	struct vm_anon *anon = l1_entry->entries[l1];
	if (anon)
		EXPECT(kmutex_acquire(&anon->anon_lock, TIMEOUT_INFINITE));

	return &l1_entry->entries[l1];
}

struct vm_anon **vm_amap_lookup(struct vm_amap *amap, voff_t offset,
				unsigned int flags)
{
	if (flags & VM_AMAP_LOCKED)
		return locked_amap_lookup(amap, offset, flags);

	guard(mutex)(&amap->lock);
	return locked_amap_lookup(amap, offset, flags);
}

// If a CoW needs-copy entry is hit, it should handle dereferencing itself.
struct vm_amap *vm_amap_copy(struct vm_amap *amap)
{
	guard(mutex)(&amap->lock);

	struct vm_amap *new_map = vm_amap_create(amap->obj);

	if (amap->l3) {
		new_map->l3 = kmalloc(sizeof(struct vm_amap_l3));
		memset(new_map->l3, 0, sizeof(struct vm_amap_l3));

		for (size_t i = 0; i < elementsof(amap->l3->entries); i++) {
			struct vm_amap_l2 *l2e = amap->l3->entries[i];
			if (!l2e)
				continue;

			struct vm_amap_l2 *new_l2e =
				kmalloc(sizeof(struct vm_amap_l2));
			memset(new_l2e, 0, sizeof(struct vm_amap_l2));

			new_map->l3->entries[i] = new_l2e;

			for (size_t j = 0; j < elementsof(l2e->entries); j++) {
				struct vm_amap_l1 *l1e = l2e->entries[j];
				if (!l1e)
					continue;

				struct vm_amap_l1 *new_l1e =
					kmalloc(sizeof(struct vm_amap_l2));
				memset(new_l1e, 0, sizeof(struct vm_amap_l1));

				new_l2e->entries[i] = new_l1e;

				for (size_t k = 0; k < elementsof(l1e->entries);
				     k++) {
					struct vm_anon *anon = l1e->entries[k];
					if (!anon)
						continue;

					vm_anon_ref(anon);

					new_l1e->entries[k] = anon;
				}
			}
		}
	}

	return new_map;
}

static struct vm_anon *vm_amap_fill_locked(struct vm_amap *amap, voff_t offset,
					   struct page **ppage,
					   unsigned int flags)
{
	struct page *page = NULL;
	if (IS_ERR(vm_lookuppage(amap->obj, offset, 0, &page))) {
		// object does not contain offset
		// -> fault should fail with SIGSEGV or some kind of OOB?
		pr_error("lookuppage returned an error\n");
		*ppage = NULL;
		return NULL;
	}

	struct vm_anon **panon =
		vm_amap_lookup(amap, offset, VM_AMAP_CREATE | VM_AMAP_LOCKED);

	// someone else was faster!
	if (*panon != NULL) {
		struct vm_anon *anon = *panon;
		// TODO: what if we page out?
		assert(anon->page == page);
		*ppage = anon->page;
		return anon;
	}

	*panon = vm_anon_create(page, offset);
	*ppage = page;
	return *panon;
}

struct vm_anon *vm_amap_fill(struct vm_amap *amap, voff_t offset,
			     struct page **ppage, unsigned int flags)
{
	if (flags & VM_AMAP_LOCKED)
		return vm_amap_fill_locked(amap, offset, ppage, flags);

	guard(mutex)(&amap->lock);
	return vm_amap_fill_locked(amap, offset, ppage, flags);
}
