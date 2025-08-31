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
#include <yak/vm/amap.h>
#include <yak/vm/object.h>
#include <yak/macro.h>
#include <yak/arch-mm.h>
#include <yak/log.h>

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

		assert(entry->object != NULL);

		struct page *page;

		struct vm_amap *amap = entry->amap;

		if (amap != NULL) {
			struct vm_anon *anon = NULL;
			struct vm_anon **anonp =
				vm_amap_lookup(amap, offset, 1);

			if (*anonp) {
				anon = *anonp;
				// TODO: page-in

			} else {
				anon = kmalloc(sizeof(struct vm_anon));
				struct page *pg;
				EXPECT(vm_lookuppage(amap->obj, offset, 0,
						     &pg));
				page_ref(pg);

				anon->page = pg;
				anon->offset = offset;
				anon->refcnt = 1;

				*anonp = anon;
			}

			assert(anon->page);
			page = anon->page;
		} else {
			EXPECT(vm_lookuppage(entry->object, offset, 0, &page));
		}

		pmap_map(&map->pmap, entry->base + offset, page_to_addr(page),
			 0, entry->protection, entry->cache);

		rwlock_release_exclusive(&map->map_lock);
		return YAK_SUCCESS;
	}

	rwlock_release_shared(&map->map_lock);
	return YAK_SUCCESS;
}
