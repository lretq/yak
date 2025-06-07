#pragma once

#include <stddef.h>
#include <stdint.h>
#include <yak/vm/pmap.h>
#include <yak/status.h>
#include <yak/tree.h>
#include <yak/vmflags.h>
#include <yak/mutex.h>

typedef enum {
	VM_OBJ_NULL = 0,
	VM_OBJ_VNODE,
	VM_OBJ_ANON,
} vm_object_type_t;

struct vm_object {
	vm_object_type_t type;
};

struct vm_map_entry {
	RBT_ENTRY(struct vm_map_entry) tree_entry;

	uintptr_t base;
	uintptr_t end;

	// union with other meta?

	uintptr_t physical_address;
	int is_physical;

	vm_prot_t protection;
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

status_t vm_map_mmio(struct vm_map *map, uintptr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, uintptr_t *out);

status_t vm_unmap(struct vm_map *map, uintptr_t va);
