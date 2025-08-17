#include <string.h>
#include <yak/heap.h>
#include <yak/macro.h>
#include <yak/vm/map.h>
#include <yak/arch-mm.h>
#include <yak/vm/pmap.h>
#include <yak/vm/pmm.h>
#include <yak/panic.h>
#include <yak/vm/kmem.h>
#include <yak-private/slab_impl.h>

static size_t kmalloc_sizes[] = { 16,  32,   64,   128,	 160,  256,  512, 640,
				  768, 1024, 2048, 3072, 4096, 5120, 8192 };
static const char *kmalloc_names[] = {
	"kmalloc_16",	"kmalloc_32",	"kmalloc_64",	"kmalloc_128",
	"kmalloc_160",	"kmalloc_256",	"kmalloc_512",	"kmalloc_640",
	"kmalloc_768",	"kmalloc_1024", "kmalloc_2048", "kmalloc_3072",
	"kmalloc_4096", "kmalloc_5120", "kmalloc_8192"
};
static kmem_cache_t *kmalloc_caches[elementsof(kmalloc_sizes)];
#define KMALLOC_MAX (kmalloc_sizes[elementsof(kmalloc_sizes) - 1])

void heap_init()
{
	vspace_add_range(&kmap()->vspace, KERNEL_HEAP_BASE, KERNEL_HEAP_LENGTH);

	kmem_init();

	for (size_t i = 0; i < elementsof(kmalloc_sizes); i++) {
		kmalloc_caches[i] = kmem_cache_create(kmalloc_names[i],
						      kmalloc_sizes[i], 0, NULL,
						      NULL, NULL, NULL, 0);
	}
}

void *kmalloc(size_t size)
{
	if (likely(size <= KMALLOC_MAX)) {
		for (size_t i = 0; i < elementsof(kmalloc_sizes); i++) {
			if (kmalloc_sizes[i] >= size) {
				void *ptr =
					kmem_cache_alloc(kmalloc_caches[i], 0);
				return ptr;
			}
		}
	}

	void *ptr = vm_kalloc(ALIGN_UP(size, PAGE_SIZE), 0);
	return ptr;
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

	if (likely(size <= KMALLOC_MAX)) {
		for (size_t i = 0; i < elementsof(kmalloc_sizes); i++) {
			if (kmalloc_sizes[i] >= size) {
				kmem_cache_free(kmalloc_caches[i], ptr);
				return;
			}
		}
	}
	vm_kfree(ptr, ALIGN_UP(size, PAGE_SIZE));
}
