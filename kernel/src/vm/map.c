#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <yak/vm/map.h>
#include <yak/mutex.h>
#include <yak/sched.h>
#include <yak/macro.h>
#include <yak/vm/pmap.h>
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

static struct vm_map kernel_map;

struct vm_map *kmap()
{
	return &kernel_map;
}

status_t vm_map_init(struct vm_map *map)
{
	RBT_INIT(vm_map_rbtree, &map->map_tree);
	kmutex_init(&map->lock);
	if (unlikely(map == &kernel_map))
		pmap_kernel_bootstrap(&map->pmap);
	return YAK_SUCCESS;
}

// TODO: proper virtual memory allocator
// -> VMEM etc...
static uintptr_t kernel_arena_base = KMAP_ARENA_BASE;

status_t vm_map_alloc(struct vm_map *map, size_t length, uintptr_t *out)
{
	assert(IS_ALIGNED_POW2(length, PAGE_SIZE));
	(void)map;
	*out = __atomic_fetch_add(&kernel_arena_base, length, __ATOMIC_RELAXED);
	return YAK_SUCCESS;
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
			   vm_inheritance_t inheritance)
{
	entry->base = base;
	entry->end = end;

	entry->offset = offset;

	entry->max_protection = entry->protection = prot;

	entry->inheritance = inheritance;

	entry->cache = VM_CACHE_DEFAULT;
}

static void insert_map_entry(struct vm_map *map, struct vm_map_entry *entry)
{
	kmutex_acquire(&map->lock, TIMEOUT_INFINITE);
	RBT_INSERT(vm_map_rbtree, &map->map_tree, entry);
	kmutex_release(&map->lock);
}

const char *entry_type(struct vm_map_entry *entry)
{
	switch (entry->type) {
	case VM_MAP_ENT_MMIO:
		return "mmio";
	case VM_MAP_ENT_OBJ:
		return "object";
	case VM_MAP_ENT_ANON:
		return "anon";
	}
	return "<unknown>";
}

void vm_map_dump(struct vm_map *map)
{
	struct vm_map_entry *entry;
	printk(0, "\t=== MAP DUMP ===\n");

	printk(0, "map: 0x%p\nentries:\n", map);
	RBT_FOREACH(entry, vm_map_rbtree, &map->map_tree)
	{
		printk(0, "(%p): 0x%lx - 0x%lx (%s)\n", entry, entry->base,
		       entry->end, entry_type(entry));
	}
}

status_t vm_unmap(struct vm_map *map, uintptr_t va)
{
	status_t ret = YAK_SUCCESS;
	kmutex_acquire(&map->lock, TIMEOUT_INFINITE);
	struct vm_map_entry *entry = vm_map_lookup_entry_locked(map, va);
	if (!entry) {
		ret = YAK_NOENT;
		goto cleanup;
	}

	RBT_REMOVE(vm_map_rbtree, &map->map_tree, entry);
	pmap_unmap_range(&map->pmap, va, entry->end - entry->base, 0);

cleanup:;
	kmutex_release(&map->lock);

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
	vaddr_t addr;
	IF_ERR((status = vm_map_alloc(map, length, &addr)))
	{
		return status;
	}

	struct vm_map_entry *entry = alloc_map_entry();
	assert(entry != NULL);

	init_map_entry(entry, 0, addr, addr + length, prot, VM_INHERIT_NONE);

	entry->type = VM_MAP_ENT_MMIO;
	entry->mmio_addr = rounded_addr;

	entry->cache = cache;

	insert_map_entry(map, entry);

	*out = addr + offset;
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
