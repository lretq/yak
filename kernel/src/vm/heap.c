#include <stdint.h>
#include <string.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/vm/map.h>
#include <yak/arch-mm.h>
#include <yak/vm/pmap.h>
#include <yak/vm/pmm.h>
#include <yak/panic.h>

static uintptr_t heap_base;
static uintptr_t heap_end;

void heap_init()
{
#define HEAP_SIZE PAGE_SIZE * 300
	EXPECT(vm_map_alloc(kmap(), HEAP_SIZE, &heap_base));
	heap_end = heap_base + HEAP_SIZE;
	for (uintptr_t i = heap_base; i < heap_end; i += PAGE_SIZE) {
		pmap_map(&kmap()->pmap, i, pmm_alloc(), 0, VM_RW,
			 VM_CACHE_DEFAULT);
	}
}

void *kmalloc(size_t size)
{
	if (unlikely(size == 0)) {
		size = 16;
	} else {
		size = ALIGN_UP(size, 16);
	}
	uintptr_t addr = __atomic_fetch_add(&heap_base, size, __ATOMIC_RELAXED);
	if (unlikely(heap_base >= heap_end)) {
		panic("Heap OOM");
	}
	return (void *)addr;
}

void *kcalloc(size_t size)
{
	void *addr = kmalloc(size);
	if (likely(addr)) {
		memset(addr, 0, size);
	}
	return addr;
}

void kfree(void *ptr, size_t size)
{
	(void)ptr;
	(void)size;
}
