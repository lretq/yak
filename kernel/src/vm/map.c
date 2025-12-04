#include "yak/vm/vmem.h"
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <yak/vm/map.h>
#include <yak/vm/object.h>
#include <yak/vm/aobj.h>
#include <yak/mutex.h>
#include <yak/rwlock.h>
#include <yak/cpudata.h>
#include <yak/sched.h>
#include <yak/macro.h>
#include <yak/vm.h>
#include <yak/vm/pmap.h>
#include <yak/vm/amap.h>
#include <yak/arch-mm.h>
#include <yak/status.h>
#include <yak/log.h>
#include <yak/types.h>
#include <yak/vmflags.h>
#include <yak/heap.h>
#include <yak/tree.h>

int vm_map_entry_cmp(const struct vm_map_entry *a, const struct vm_map_entry *b)
{
	if (a->base == b->base)
		return 0;
	return a->base > b->base ? 1 : -1;
}

RBT_PROTOTYPE(vm_map_rbtree, vm_map_entry, tree_entry, vm_map_entry_cmp);
RBT_GENERATE(vm_map_rbtree, vm_map_entry, tree_entry, vm_map_entry_cmp);

struct vm_map *kernel_map = &kproc0.map;

struct vm_map *kmap()
{
	return kernel_map;
}

status_t vm_map_init(struct vm_map *map)
{
	rwlock_init(&map->map_lock, "map_lock");

	RBT_INIT(vm_map_rbtree, &map->map_tree);

	if (unlikely(map == kernel_map)) {
		pmap_kernel_bootstrap(&map->pmap);
	} else {
		pmap_init(&map->pmap);
	}

	return YAK_SUCCESS;
}

void vm_map_free(struct vm_map *map, vaddr_t addr, size_t length)
{
}

static struct vm_map_entry *alloc_map_entry()
{
	// XXX: slab
	return kmalloc(sizeof(struct vm_map_entry));
}

[[maybe_unused]]
static void free_map_entry(struct vm_map_entry *entry)
{
	kfree(entry, sizeof(struct vm_map_entry));
}

static void init_map_entry(struct vm_map_entry *entry, voff_t offset,
			   vaddr_t base, vaddr_t end, vm_prot_t prot,
			   vm_inheritance_t inheritance, vm_cache_t cache,
			   unsigned short entry_type)
{
	entry->base = base;
	entry->end = end;

	entry->type = entry_type;

	entry->offset = offset;

	entry->is_cow = false;
	entry->amap_needs_copy = false;

	entry->amap = NULL;

	entry->max_protection = entry->protection = prot;

	entry->inheritance = inheritance;

	entry->cache = cache;
}

const char *entry_type(struct vm_map_entry *entry)
{
	switch (entry->type) {
	case VM_MAP_ENT_MMIO:
		return "mmio";
	case VM_MAP_ENT_OBJ:
		if (entry->object) {
			return "object";
		} else {
			return "anon";
		}
	}
	return "<unknown>";
}

#ifdef CONFIG_DEBUG
void vm_map_dump(struct vm_map *map)
{
	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE, RWLOCK_GUARD_SHARED);

	struct vm_map_entry *entry;
	printk(0, "\t=== MAP DUMP ===\n");

	printk(0, "map: 0x%p\nentries:\n", map);
	RBT_FOREACH(entry, vm_map_rbtree, &map->map_tree)
	{
		printk(0, "(%p): 0x%lx - 0x%lx (%s)\n", entry, entry->base,
		       entry->end, entry_type(entry));
	}
}
#endif

status_t vm_unmap(struct vm_map *map, uintptr_t va, size_t length, int flags)
{
	assert(map);

	va = ALIGN_DOWN(va, PAGE_SIZE);
	length = ALIGN_UP(length, PAGE_SIZE);

	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE,
		      (flags & VM_MAP_LOCK_HELD) ? RWLOCK_GUARD_SKIP :
						   RWLOCK_GUARD_EXCLUSIVE);

	vaddr_t start = va;
	vaddr_t end = va + length;

	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, start);
	if (!entry) {
		return YAK_NOENT;
	}

	while (entry && entry->base < end) {
		struct vm_map_entry *next = RBT_NEXT(vm_map_rbtree, entry);
		RBT_REMOVE(vm_map_rbtree, &map->map_tree, entry);

		if (entry->end <= start) {
			entry = next;
			continue;
		}

		if (entry->base >= end) {
			break;
		}

		if (start <= entry->base && end >= entry->end) {
			// remove entire entry

			// we cannot use pmap_unmap_range_and_free,
			// as we dont know what memory we mapped
			pmap_unmap_range(&map->pmap, entry->base,
					 entry->end - entry->base, 0);

			if (entry->type == VM_MAP_ENT_OBJ) {
				if (entry->amap)
					vm_amap_deref(entry->amap);

				vm_object_deref(entry->object);
			}

			free_map_entry(entry);
			entry = NULL;
		} else if (start <= entry->base && end < entry->end) {
			// overlaps on left side of entry

			size_t unmap_len = end - entry->base;

			pmap_unmap_range(&map->pmap, entry->base, unmap_len, 0);

			entry->base = end;
			entry->offset += unmap_len;
		} else if (start > entry->base && end >= entry->end) {
			// overlaps on right side of entry

			size_t unmap_len = entry->end - start;
			pmap_unmap_range(&map->pmap, start, unmap_len, 0);

			entry->end = start;
			// dont cahnge offset: left side stays the same
		} else {
			// split in middle of entry

			struct vm_map_entry *right = alloc_map_entry();
			if (!right) {
				return YAK_OOM;
			}

			init_map_entry(right,
				       entry->offset + (end - entry->base), end,
				       entry->end, entry->max_protection,
				       entry->inheritance, entry->cache,
				       entry->type);
			right->protection = entry->protection;

			if (right->type == VM_MAP_ENT_OBJ) {
				if (right->amap)
					vm_amap_ref(right->amap);
				vm_object_ref(right->object);
			}

			// truncate left part
			entry->end = start;

			RBT_INSERT(vm_map_rbtree, &map->map_tree, right);

			pmap_unmap_range(&map->pmap, start, end - start, 0);
		}

		if (entry != NULL) {
			// reinsert entry with updated bounds
			RBT_INSERT(vm_map_rbtree, &map->map_tree, entry);
		}

		entry = next;
	}

	return YAK_SUCCESS;
}

// lowest node where element->base >= base
static struct vm_map_entry *map_lower_bound(struct vm_map *map, vaddr_t addr)
{
	struct vm_map_entry *res = NULL;
	struct vm_map_entry *cur = RBT_ROOT(vm_map_rbtree, &map->map_tree);

	while (cur) {
		if (addr > cur->base) {
			// too little: have to go right
			cur = RBT_RIGHT(vm_map_rbtree, cur);
		} else {
			res = cur;
			// but there could be a closer one still
			cur = RBT_LEFT(vm_map_rbtree, cur);
		}
	}

	return res;
}

static status_t alloc_map_range_locked(struct vm_map *map, vaddr_t hint,
				       size_t length, vm_prot_t prot,
				       vm_inheritance_t inheritance,
				       vm_cache_t cache, voff_t offset,
				       unsigned short entry_type, int flags,
				       struct vm_map_entry **entry)
{
	vaddr_t min_addr, max_addr;

	if (map == kmap()) {
		min_addr = KERNEL_VA_BASE;
		max_addr = KERNEL_VA_END;
	} else {
		min_addr = USER_VA_BASE;
		max_addr = USER_VA_END;
	}

	if (length == 0) {
		return YAK_INVALID_ARGS;
	}

	if (length > max_addr - min_addr) {
		return YAK_NOSPACE;
	}

	struct vm_map_entry *next, *prev;
	next = prev = NULL;
	vaddr_t base = hint;

	// find first node >= hint
	next = map_lower_bound(map, base);

	if (flags & VM_MAP_FIXED) {
		// tried to allocate either null page or user va
		if (base < min_addr)
			return YAK_INVALID_ARGS;
		// overflow
		if (base + length < base)
			return YAK_INVALID_ARGS;
		// tried to allocate beyond allowed range
		// (e.g. > 32 bit or in kernel range)
		if (base + length > max_addr)
			return YAK_INVALID_ARGS;

		prev = next ? RBT_PREV(vm_map_rbtree, next) :
			      RBT_MAX(vm_map_rbtree, &map->map_tree);

		if ((prev && prev->end > base) ||
		    (next && next->base < base + length)) {
			if (!(flags & VM_MAP_OVERWRITE))
				return YAK_EXISTS;

			vm_unmap(map, base, length, VM_MAP_LOCK_HELD);
		}

		goto found;
	}

	if (base < min_addr)
		base = min_addr;

	if (base > max_addr)
		base = max_addr;

	if (RBT_EMPTY(vm_map_rbtree, &map->map_tree)) {
		// empty map: try to satisfy mapping exactly at hint
		if (base + length > max_addr) {
			base = min_addr;
			if (base + length > max_addr) {
				return YAK_NOSPACE;
			}
		}
		goto found;
	}

	next = map_lower_bound(map, base);
	prev = next ? RBT_PREV(vm_map_rbtree, next) : NULL;

	if (!prev) {
		assert(next);

		vaddr_t gap_start = min_addr;
		if (next->base >= gap_start + length) {
			base = gap_start;
			goto found;
		}

		prev = next;
		next = RBT_NEXT(vm_map_rbtree, next);
	}

	bool wrapped = false;
	for (;;) {
		vaddr_t gap_start = prev ? prev->end : min_addr;
		vaddr_t gap_end = next ? next->base : max_addr;

		// try to respect the hint if it lies inside the gap
		gap_start = MAX(gap_start, base);

		if (gap_end - gap_start >= length) {
			base = gap_start;
			goto found;
		}

		prev = next;
		next = RBT_NEXT(vm_map_rbtree, next);
		if (!next) {
			if (prev->end + length <= max_addr) {
				base = prev->end;
				goto found;
			}

			if (hint == 0 || wrapped) {
				return YAK_NOSPACE;
			}

			wrapped = true;
			base = min_addr;
			prev = NULL;
			next = RBT_MIN(vm_map_rbtree, &map->map_tree);
		}
	}

found:
	assert(base >= min_addr || base + length < max_addr);

	*entry = alloc_map_entry();
	if (!entry)
		return YAK_OOM;

	init_map_entry(*entry, offset, base, base + length, prot, inheritance,
		       cache, entry_type);
	RBT_INSERT(vm_map_rbtree, &map->map_tree, *entry);

	return YAK_SUCCESS;
}

status_t vm_map_reserve(struct vm_map *map, vaddr_t hint, size_t length,
			int flags, vaddr_t *out)
{
	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE, RWLOCK_GUARD_EXCLUSIVE);

	struct vm_map_entry *ent;
	status_t rv = alloc_map_range_locked(map, hint, length, 0, 0, 0, 0,
					     VM_MAP_ENT_RESERVED, flags, &ent);
	*out = IS_OK(rv) ? ent->base : 0;
	return rv;
}

status_t vm_map_mmio(struct vm_map *map, paddr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, vaddr_t *out)
{
	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE, RWLOCK_GUARD_EXCLUSIVE);

	paddr_t rounded_addr = ALIGN_DOWN(device_addr, PAGE_SIZE);
	size_t offset = device_addr - rounded_addr;
	// aligned length
	length = ALIGN_UP(offset + length, PAGE_SIZE);

	struct vm_map_entry *entry;
	status_t rv = alloc_map_range_locked(map, 0, length, prot,
					     VM_INHERIT_NONE, cache, offset,
					     VM_MAP_ENT_MMIO, 0, &entry);
	IF_ERR(rv)
	{
		return rv;
	}

	entry->mmio_addr = rounded_addr;

	vaddr_t addr = entry->base;
	*out = addr + offset;

	return YAK_SUCCESS;
}

status_t vm_map(struct vm_map *map, struct vm_object *obj, size_t length,
		voff_t offset, vm_prot_t prot, vm_inheritance_t inheritance,
		vm_cache_t cache, vaddr_t hint, int flags, vaddr_t *out)
{
	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE, RWLOCK_GUARD_EXCLUSIVE);

	struct vm_map_entry *entry;
	status_t rv = alloc_map_range_locked(map, hint, length, prot,
					     inheritance, cache, offset,
					     VM_MAP_ENT_OBJ, flags, &entry);
	IF_ERR(rv)
	{
		return rv;
	}

	vaddr_t addr = entry->base;

	if (obj == NULL) {
		obj = vm_aobj_create();
	}
	entry->object = obj;

	entry->is_cow = (inheritance == VM_INHERIT_COPY);
	entry->amap_needs_copy = entry->is_cow;
	entry->amap = NULL;

	if (flags & VM_MAP_PREFILL) {
		for (voff_t off = 0; off < length; off += PAGE_SIZE) {
			EXPECT(vm_handle_fault(map, addr + off,
					       VM_FAULT_PREFILL));
		}
	}

	*out = addr;
	return YAK_SUCCESS;
}

struct vm_map_entry *vm_map_lookup_entry_locked(struct vm_map *map,
						uintptr_t address)
{
	struct vm_map_entry *entry = RBT_ROOT(vm_map_rbtree, &map->map_tree);
	while (entry) {
		if (address < entry->base) {
			entry = RBT_LEFT(vm_map_rbtree, entry);
		} else if (address >= entry->end) {
			entry = RBT_RIGHT(vm_map_rbtree, entry);
		} else {
			assert(address >= entry->base && address <= entry->end);
			return entry;
		}
	}
	return NULL;
}

void vm_map_activate(struct vm_map *map)
{
	pmap_activate(&map->pmap);
	curcpu().current_map = map;
}

void vm_map_tmp_switch(struct vm_map *map)
{
	curthread()->vm_ctx = map;
	vm_map_activate(map);
}

void vm_map_tmp_disable()
{
	assert(curthread()->vm_ctx);
	curthread()->vm_ctx = NULL;
}

status_t vm_map_fork(struct vm_map *from, struct vm_map *to)
{
	vm_map_init(to);

	guard(rwlock)(&from->map_lock, TIMEOUT_INFINITE,
		      RWLOCK_GUARD_EXCLUSIVE);

	struct vm_map_entry *elm;
	VM_MAP_FOREACH(elm, &from->map_tree)
	{
		if (elm->inheritance == VM_INHERIT_NONE)
			continue;

		struct vm_map_entry *new_ent = alloc_map_entry();
		new_ent->base = elm->base;
		new_ent->end = elm->end;

		new_ent->type = elm->type;

		new_ent->offset = elm->offset;

		new_ent->protection = elm->protection;
		new_ent->max_protection = elm->max_protection;

		new_ent->inheritance = elm->inheritance;
		new_ent->cache = elm->cache;

		new_ent->is_cow = elm->is_cow;
		new_ent->amap_needs_copy = new_ent->is_cow;
		new_ent->amap = elm->amap;

		if (elm->type == VM_MAP_ENT_OBJ) {
			// the only type to have the special fork semantics.
			// handled in amap.c/fault.c
			new_ent->object = elm->object;
		} else if (elm->type == VM_MAP_ENT_MMIO) {
			// entry maps device or simply physical memory.
			// no need to copy anything.
			new_ent->mmio_addr = elm->mmio_addr;
		} else if (elm->type == VM_MAP_ENT_RESERVED) {
			// nop: this is simply empty space!
		}

		RBT_INSERT(vm_map_rbtree, &from->map_tree, new_ent);
	}

	return YAK_SUCCESS;
}
