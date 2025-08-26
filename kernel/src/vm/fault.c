#include <stddef.h>
#include <string.h>
#include <yak/status.h>
#include <yak/types.h>
#include <yak/sched.h>
#include <yak/vm/map.h>
#include <yak/mutex.h>
#include <yak/rwlock.h>
#include <yak/heap.h>
#include <yak/vm/pmap.h>
#include <yak/vm/pmm.h>
#include <yak/macro.h>
#include <yak/arch-mm.h>
#include <yak/log.h>

struct vm_anon **vm_amap_lookup(struct vm_amap *amap, voff_t offset, int create)
{
	assert(amap);

	size_t l3, l2, l1;
	// mimic a 3level page table
	size_t pg = offset >> 12;
	l3 = (pg >> 18) & 511;
	l2 = (pg >> 9) & 511;
	l1 = pg & 511;

	struct vm_amap_l2 *l2_entry = amap->entries[l3];
	if (!l2_entry) {
		if (!create)
			return NULL;

		l2_entry = kmalloc(sizeof(struct vm_amap_l2));
		memset(l2_entry, 0, sizeof(struct vm_amap_l2));
		amap->entries[l3] = l2_entry;
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

size_t n_pagefaults;

// TODO: fault_flags are also used in map.c:VM_PREFILL!!!
status_t vm_handle_fault(struct vm_map *map, vaddr_t address,
			 unsigned long fault_flags)
{
	if ((fault_flags & VM_PREFILL) == 0)
		__atomic_fetch_add(&n_pagefaults, 1, __ATOMIC_RELAXED);

	(void)fault_flags;

	address = ALIGN_DOWN(address, PAGE_SIZE);

	rwlock_acquire_shared(&map->map_lock, TIMEOUT_INFINITE);
	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, address);

	if (!entry) {
		rwlock_release_shared(&map->map_lock);
		if (address >= 0x0 && address < PAGE_SIZE)
			return YAK_NULL_DEREF;

		return YAK_NOENT;
	}

	voff_t offset = address - entry->base + entry->offset;

	if (entry->type == VM_MAP_ENT_MMIO) {
		pmap_map(&map->pmap, entry->base + offset,
			 entry->mmio_addr + offset, 0, entry->protection,
			 entry->cache);
	} else {
		rwlock_upgrade_to_exclusive(&map->map_lock);

		if (entry->object == NULL) {
			if (entry->amap == NULL) {
				entry->amap = vm_amap_create();
			}
			assert(entry->amap);
			struct vm_anon **anonp =
				vm_amap_lookup(entry->amap, offset, 1);
			struct vm_anon *anon = *anonp;

			if (!anon) {
				// create zero-filled page
				anon = kmalloc(sizeof(struct vm_anon));
				anon->page = pmm_alloc_order(0);
				assert(anon->page);
				page_zero(anon->page, 0);
				anon->offset = offset;
				anon->refcnt = 1;

				*anonp = anon;
			}

#if 0
			pr_info("anon %p\n", anon);
			pr_info("anon page %lx\n", page_to_addr(anon->page));
#endif

			assert(anon);
			assert(anon->page);

			pmap_map(&map->pmap, entry->base + offset,
				 anon->page->pfn << PAGE_SHIFT, 0,
				 entry->protection, entry->cache);

		} else {
			// we DO have a backing object
			panic("TODO");
		}

		rwlock_release_exclusive(&map->map_lock);
		return YAK_SUCCESS;
	}

	rwlock_release_shared(&map->map_lock);
	return YAK_SUCCESS;
}
