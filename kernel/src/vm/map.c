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
	RBT_INIT(vm_map_rbtree, &map->map_tree);
	rwlock_init(&map->map_lock, "map_lock");

	if (unlikely(map == kernel_map)) {
		pmap_kernel_bootstrap(&map->pmap);
	} else {
		pmap_init(&map->pmap);
		map->arena = vmem_init(NULL, "map", (void *)USER_VA_BASE,
				       USER_VA_END - USER_VA_BASE, PAGE_SIZE,
				       NULL, NULL, NULL, PAGE_SIZE * 8,
				       VM_SLEEP);
	}

	return YAK_SUCCESS;
}

status_t vm_map_alloc(struct vm_map *map, size_t length, vaddr_t *out)
{
	assert(IS_ALIGNED_POW2(length, PAGE_SIZE));
	void *addr = vmem_alloc(map->arena, length, VM_SLEEP);
	*out = (vaddr_t)addr;
	if (!addr) {
		vmem_dump(map->arena);
	}
	return addr != NULL ? YAK_SUCCESS : YAK_NOSPACE;
}

status_t vm_map_xalloc(struct vm_map *map, size_t length, int exact,
		       vaddr_t *out)
{
	assert(IS_ALIGNED_POW2(length, PAGE_SIZE));
	void *minaddr = NULL, *maxaddr = NULL;
	if (exact) {
		minaddr = (void *)*out;
		maxaddr = (void *)(*out + length);
	}
	void *addr =
		vmem_xalloc(map->arena, length, 0, 0, 0, minaddr, maxaddr, 0);
	*out = (vaddr_t)addr;
	if (!addr) {
		vmem_dump(map->arena);
	}
	return addr != NULL ? YAK_SUCCESS : YAK_NOSPACE;
}

void vm_map_free(struct vm_map *map, vaddr_t addr, size_t length)
{
	vmem_free(map->arena, (void *)addr, length);
}

void vm_map_xfree(struct vm_map *map, vaddr_t addr, size_t length)
{
	vmem_xfree(map->arena, (void *)addr, length);
}

static struct vm_map_entry *alloc_map_entry()
{
	return kmalloc(sizeof(struct vm_map_entry));
}

[[maybe_unused]]
static void free_map_entry(struct vm_map_entry *entry)
{
	kfree(entry, sizeof(struct vm_map_entry));
}

static void init_map_entry(struct vm_map_entry *entry, voff_t offset,
			   vaddr_t base, vaddr_t end, vm_prot_t prot,
			   vm_inheritance_t inheritance, vm_cache_t cache)
{
	entry->base = base;
	entry->end = end;

	entry->offset = offset;

	entry->amap = NULL;

	entry->max_protection = entry->protection = prot;

	entry->inheritance = inheritance;

	entry->cache = cache;
}

static void insert_map_entry(struct vm_map *map, struct vm_map_entry *entry)
{
	guard(rwlock)(&map->map_lock, TIMEOUT_INFINITE, RWLOCK_GUARD_EXCLUSIVE);

	RBT_INSERT(vm_map_rbtree, &map->map_tree, entry);
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

status_t vm_unmap(struct vm_map *map, uintptr_t va)
{
	EXPECT(rwlock_acquire_exclusive(&map->map_lock, TIMEOUT_INFINITE));

	status_t ret = YAK_SUCCESS;

	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, va);
	if (!entry) {
		ret = YAK_NOENT;
		goto cleanup;
	}

	RBT_REMOVE(vm_map_rbtree, &map->map_tree, entry);

	// give back the reserved space
	vm_map_xfree(map, va, entry->end - entry->base);

	// we cannot use pmap_unmap_range_and_free,
	// as we dont know what memory we mapped
	pmap_unmap_range(&map->pmap, va, entry->end - entry->base, 0);

	/* MMIO mappings only map memory */
	if (entry->type == VM_MAP_ENT_MMIO)
		goto cleanup;

	if (entry->amap)
		vm_amap_deref(entry->amap);

	vm_object_deref(entry->object);

cleanup:
	rwlock_release_exclusive(&map->map_lock);

	if (entry)
		free_map_entry(entry);

	return ret;
}

status_t vm_unmap_mmio(struct vm_map *map, vaddr_t va)
{
	return vm_unmap(map, ALIGN_DOWN(va, PAGE_SIZE));
}

status_t vm_map_mmio(struct vm_map *map, paddr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, vaddr_t *out)
{
	paddr_t rounded_addr = ALIGN_DOWN(device_addr, PAGE_SIZE);
	size_t offset = device_addr - rounded_addr;
	// aligned length
	length = ALIGN_UP(offset + length, PAGE_SIZE);

	status_t status;

	vaddr_t addr = 0;
	IF_ERR((status = vm_map_xalloc(map, length, 0, &addr)))
	{
		return status;
	}

	struct vm_map_entry *entry = alloc_map_entry();
	assert(entry != NULL);

	init_map_entry(entry, 0, addr, addr + length, prot, VM_INHERIT_NONE,
		       cache);

	entry->type = VM_MAP_ENT_MMIO;
	entry->mmio_addr = rounded_addr;

	insert_map_entry(map, entry);

	*out = addr + offset;
	return YAK_SUCCESS;
}

status_t vm_map(struct vm_map *map, struct vm_object *obj, size_t length,
		voff_t offset, int map_exact, vm_prot_t prot,
		vm_inheritance_t inheritance, vm_cache_t cache, vaddr_t hint,
		vaddr_t *out)
{
	status_t status;

	vaddr_t addr = hint;

	IF_ERR((status = vm_map_xalloc(map, length, map_exact, &addr)))
	{
		return status;
	}

	struct vm_map_entry *entry = alloc_map_entry();
	assert(entry);
	init_map_entry(entry, offset, addr, addr + length, prot, inheritance,
		       cache);

	entry->type = VM_MAP_ENT_OBJ;

	if (obj == NULL) {
		obj = vm_aobj_create();
	}
	entry->object = obj;

	insert_map_entry(map, entry);

	// used esp. for kernel stacks
	// could also be used for MMIO?
	if (prot & VM_PREFILL) {
		for (vaddr_t off = 0; off < length; off += PAGE_SIZE) {
			EXPECT(vm_handle_fault(map, addr + off, VM_PREFILL));
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
