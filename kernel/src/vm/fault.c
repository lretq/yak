#include <yak/status.h>
#include <yak/types.h>
#include <yak/vm/map.h>
#include <yak/mutex.h>
#include <yak/vm/pmap.h>
#include <yak/log.h>

status_t vm_handle_fault(struct vm_map *map, vaddr_t address,
			 unsigned long fault_flags)
{
	(void)fault_flags;

	address = ALIGN_DOWN(address, PAGE_SIZE);

	kmutex_acquire(&map->lock);
	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, address);

	if (!entry) {
		kmutex_release(&map->lock);
		return YAK_NOENT;
	}

	size_t offset = address - entry->base;

	if (entry->type == VM_MAP_ENT_MMIO) {
		pmap_map(&map->pmap, entry->base + offset,
			 entry->mmio_addr + offset, 0, entry->protection,
			 entry->cache);
	} else {
		kmutex_release(&map->lock);
		return YAK_NOT_IMPLEMENTED;
	}

	kmutex_release(&map->lock);
	return YAK_SUCCESS;
}
