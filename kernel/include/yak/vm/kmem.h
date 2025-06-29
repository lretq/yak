#pragma once

#include <stddef.h>
#include <yak/status.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KM_SLEEP (1 << 0)
#define KM_NOSLEEP (1 << 1)
#define KMC_QCACHE (1 << 2)

typedef struct kmem_cache kmem_cache_t;

struct vmem;

kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize, size_t align,
				int (*constructor)(void *, void *),
				void (*destructor)(void *, void *), void *ctx,
				struct vmem *vmp, int kmflags);

status_t kmem_cache_destroy(kmem_cache_t *cp);

void *kmem_cache_alloc(kmem_cache_t *cp, int kmflags);
void kmem_cache_free(kmem_cache_t *cp, void *obj);

#ifdef __cplusplus
}
#endif
