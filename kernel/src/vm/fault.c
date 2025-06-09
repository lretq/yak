#include <yak/status.h>
#include <yak/types.h>
#include <yak/vm/map.h>
#include <yak/mutex.h>
#include <yak/vm/pmap.h>

status_t vm_handle_fault(struct vm_map *map, vaddr_t address,
			 unsigned long fault_flags)
{
	(void)fault_flags;

	kmutex_acquire(&map->lock);
	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, address);

	if (!entry) {
		kmutex_release(&map->lock);
		return YAK_NOENT;
	}

	size_t offset = ALIGN_DOWN(address - entry->base, PAGE_SIZE);

	if (entry->is_physical) {
		pmap_map(&map->pmap, entry->base + offset,
			 entry->physical_address + offset, 0, entry->protection,
			 entry->cache);
	}

	kmutex_release(&map->lock);
	return YAK_SUCCESS;
}
