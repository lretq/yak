#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <yak/cleanup.h>

void kmalloc_init();
void vmem_earlyinit();

void *vm_kalloc(size_t size, int flags);
void vm_kfree(void *ptr, size_t size);

// on init time caches of various sizes are created,
// the kmalloc suite operate upon those
void *kmalloc(size_t size);
void kfree(void *ptr, size_t size);
void *kcalloc(size_t count, size_t size);

DEFINE_CLEANUP_CLASS(
	autofree,
	{
		void *p;
		size_t size;
	},
	{
		if (ctx->p)
			kfree(ctx->p, ctx->size);
	},
	{ RET(autofree, ptr, size); }, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif
