#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <yak/vm/map.h>
#include <yak/macro.h>
#include <yak/arch-mm.h>
#include <yak/status.h>

static struct vm_map kernel_map;

struct vm_map *kmap()
{
	return &kernel_map;
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
