#pragma once

#include <stddef.h>
#include <stdint.h>
#include <yak/vm/pmap.h>
#include <yak/status.h>
#include <yak/tree.h>
#include <yak/vmflags.h>
#include <yak/types.h>
#include <yak/mutex.h>


struct vm_object {
	struct vm_pagerops *ops;
};


enum {
	VM_MAP_ENT_MMIO = 1,
	VM_MAP_ENT_OBJ,
	VM_MAP_ENT_ANON,
};

struct vm_map_entry {
	RBT_ENTRY(struct vm_map_entry) tree_entry;

	vaddr_t base; /* start address */
	vaddr_t end; /* end address exclusive */

	unsigned short type;

	union {
		paddr_t mmio_addr; /* backing physical device memory */
		struct vm_object *object; /* backing object */
	};
	voff_t offset; /* offset into backing store */

	vm_prot_t protection;
	vm_prot_t max_protection;

	vm_inheritance_t inheritance;

	vm_cache_t cache;
};

struct vm_map {
	RBT_HEAD(vm_map_rbtree, struct vm_map_entry) map_tree;
	struct kmutex lock;

	struct pmap pmap;
};

struct vm_map *kmap();

status_t vm_map_init(struct vm_map *map);

status_t vm_map_alloc(struct vm_map *map, size_t length, uintptr_t *out);

struct vm_map_entry *vm_map_lookup_entry_locked(struct vm_map *map,
						uintptr_t address);

// setup MMIO mapping
status_t vm_map_mmio(struct vm_map *map, uintptr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, uintptr_t *out);

status_t vm_map(struct vm_map *map, struct vm_object *obj,
		vm_prot_t initial_prot, vm_inheritance_t inheritance,
		uintptr_t *out);

status_t vm_unmap(struct vm_map *map, uintptr_t va);
