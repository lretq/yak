#include "yak/log.h"
#include <string.h>
#include <assert.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/vm/map.h>
#include <yak/arch-mm.h>
#include <yak/vm/pmap.h>
#include <yak/vm/pmm.h>
#include <yak/panic.h>
#include <yak/vm/kmem.h>

static size_t kmalloc_sizes[] = { 16,  32,   64,   128,	 160,  256,  512, 640,
				  768, 1024, 2048, 3072, 4096, 5120, 8192 };
static char *kmalloc_names[] = {
	"kmalloc_16",	"kmalloc_32",	"kmalloc_64",	"kmalloc_128",
	"kmalloc_160",	"kmalloc_256",	"kmalloc_512",	"kmalloc_640",
	"kmalloc_768",	"kmalloc_1024", "kmalloc_2048", "kmalloc_3072",
	"kmalloc_4096", "kmalloc_5120", "kmalloc_8192"
};
static kmem_cache_t *kmalloc_caches[elementsof(kmalloc_sizes)];
#define KMALLOC_MAX (kmalloc_sizes[elementsof(kmalloc_sizes) - 1])

void kmalloc_init()
{
	for (size_t i = 0; i < elementsof(kmalloc_sizes); i++) {
		pr_debug("create cache for size %lx\n", kmalloc_sizes[i]);
		kmalloc_caches[i] = kmem_cache_create(kmalloc_names[i],
						      kmalloc_sizes[i], 0, NULL,
						      NULL, NULL, NULL, NULL,
						      KM_SLEEP);
	}
}

typedef struct alloc_meta {
	size_t size;
	kmem_cache_t *cache;
} alloc_meta_t;
_Static_assert(sizeof(alloc_meta_t) == 16, "bad align");

void *kmalloc(size_t size)
{
	size = size + sizeof(alloc_meta_t);

	alloc_meta_t *ptr = NULL;
	if (likely(size <= KMALLOC_MAX)) {
		for (size_t i = 0; i < elementsof(kmalloc_sizes); i++) {
			if (kmalloc_sizes[i] >= size) {
				ptr = kmem_cache_alloc(kmalloc_caches[i], 0);
				ptr->cache = kmalloc_caches[i];
				break;
			}
		}
	} else {
		size = ALIGN_UP(size, PAGE_SIZE);
		ptr = vm_kalloc(size, 0);
		ptr->cache = NULL;
	}

	if (unlikely(!ptr))
		return NULL;

	ptr->size = size;

	return (void *)(ptr + 1);
}

void *kcalloc(size_t count, size_t size)
{
	void *addr = kmalloc(count * size);
	if (likely(addr)) {
		memset(addr, 0, count * size);
	}
	return addr;
}

void kfree(void *ptr, size_t size)
{
	if (unlikely(ptr == NULL))
		return;

	alloc_meta_t *meta =
		(alloc_meta_t *)((uintptr_t)ptr - sizeof(alloc_meta_t));

	if (size != 0) {
		size = size + sizeof(alloc_meta_t);
		if (meta->size != size)
			pr_debug("size=%ld meta=%ld\n", size, meta->size);
		assert(meta->size == size);
	}

	if (likely(meta->cache)) {
		assert(meta->cache);
		kmem_cache_free(meta->cache, meta);
	} else {
		vm_kfree(meta, meta->size);
	}
}
