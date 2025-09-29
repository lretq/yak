#include "yak/vm/page.h"
#include <yak/log.h>
#include <assert.h>
#include <string.h>
#include <yak/mutex.h>
#include <yak/macro.h>
#include <yak/sched.h>
#include <yak/cleanup.h>
#include <yak/vm/map.h>
#include <yak/vm/kmem.h>
#include <yak/vm/pmm.h>
#include <yak/vm/pmap.h>
#include <yak/arch-mm.h>
#include <yak-private/slab_impl.h>

#define KMEM_BOOTSTRAP (1 << 8)
#define KMEM_MAXPAGES 32

void *vm_kalloc(size_t size, [[maybe_unused]] int flags)
{
	size = ALIGN_UP(size, PAGE_SIZE);
	// TODO: replace with anon VM object, to allow for swapping?
	vaddr_t addr = 0;
	EXPECT(vm_map_alloc(kmap(), size, &addr));
	for (size_t i = 0; i < size; i += PAGE_SIZE) {
		struct page *pg = pmm_alloc_order(0);
		pmap_map(&kmap()->pmap, addr + i, page_to_addr(pg), 0, VM_RW,
			 VM_CACHE_DEFAULT);
	}
	return (void *)addr;
}

void vm_kfree(void *ptr, size_t size)
{
	vaddr_t addr = (vaddr_t)ptr;
	size = ALIGN_UP(size, PAGE_SIZE);
	for (size_t i = 0; i < size; i += PAGE_SIZE) {
		pmap_unmap(&kmap()->pmap, addr + size, 0);
	}
	vm_map_free(kmap(), addr, size);
}

static kmem_cache_t *kmem_cache_arena = NULL;
static kmem_cache_t *kmem_slab_arena = NULL;
static kmem_cache_t *kmem_bufctl_arena = NULL;

static kmem_slab_t *small_slab_create(kmem_cache_t *cp, int flags)
{
	kmem_slab_t *sp;
	void *pg;

	pg = vm_kalloc(PAGE_SIZE, flags);
	sp = (kmem_slab_t *)((uintptr_t)pg + PAGE_SIZE - sizeof(kmem_slab_t));
	SLIST_INIT(&sp->freelist);
	sp->refcnt = 0;
	sp->parent_cache = cp;
	sp->base = NULL;
	for (size_t i = 0; i < cp->slabcap; i++) {
		kmem_bufctl_t *bp =
			(kmem_bufctl_t *)((uintptr_t)pg + (i * cp->chunksize));
		SLIST_INSERT_HEAD(&sp->freelist, (kmem_bufctl_t *)(bp), entry);
	}

	return sp;
}

static kmem_slab_t *large_slab_create(kmem_cache_t *cp, int flags)
{
	kmem_slab_t *sp;
	sp = kmem_cache_alloc(kmem_slab_arena, flags);
	assert(sp);
	SLIST_INIT(&sp->freelist);
	sp->refcnt = 0;
	sp->parent_cache = cp;

	void *base = vm_kalloc(cp->slabsize, flags);
	sp->base = base;

	for (size_t i = 0; i < cp->slabcap; i++) {
		kmem_large_bufctl_t *bp =
			kmem_cache_alloc(kmem_bufctl_arena, flags);
		bp->base = (void *)((uintptr_t)base + cp->chunksize * i);
		bp->slab = sp;
		SLIST_INSERT_HEAD(&sp->freelist, &bp->bufctl, entry);
	}

	return sp;
}

static void *slab_alloc(kmem_slab_t *sp)
{
	kmem_bufctl_t *bufctl = SLIST_FIRST(&sp->freelist);
	SLIST_REMOVE_HEAD(&sp->freelist, entry);
	sp->refcnt++;
	if (sp->refcnt == sp->parent_cache->slabcap) {
		LIST_REMOVE(sp, entry);
		LIST_INSERT_HEAD(&sp->parent_cache->full_slablist, sp, entry);
	}
	if (sp->base == NULL)
		return (void *)bufctl;
	SLIST_INSERT_HEAD(&sp->parent_cache->buflist, bufctl, entry);
	return ((kmem_large_bufctl_t *)bufctl)->base;
}

static void slab_free(kmem_slab_t *sp, kmem_bufctl_t *bufctl)
{
	assert(sp);
	assert(bufctl);
	if (sp->refcnt == sp->parent_cache->slabcap) {
		LIST_REMOVE(sp, entry);
		LIST_INSERT_HEAD(&sp->parent_cache->slablist, sp, entry);
	}
	sp->refcnt--;

	if (sp->base != NULL) {
		SLIST_REMOVE(&sp->parent_cache->buflist, bufctl, kmem_bufctl,
			     entry);
	}

	SLIST_INSERT_HEAD(&sp->freelist, bufctl, entry);
}

void *kmem_cache_alloc(kmem_cache_t *cp, int kmflags)
{
	assert(cp);
	guard(mutex)(&cp->mutex);

	if (LIST_EMPTY(&cp->slablist)) {
		kmem_slab_t *slab;
		if (cp->chunksize <= KMEM_SMALL_MAX) {
			slab = small_slab_create(cp, kmflags);
		} else {
			slab = large_slab_create(cp, kmflags);
		}
		LIST_INSERT_HEAD(&cp->slablist, slab, entry);
	}

	kmem_slab_t *sp = LIST_FIRST(&cp->slablist);
	assert(sp->refcnt < cp->slabcap);
	void *obj = slab_alloc(sp);
	if (cp->constructor != NULL) {
		assert(cp->constructor(obj, cp->ctx) == 0);
	}

	return obj;
}

void kmem_cache_free(kmem_cache_t *cp, void *obj)
{
	guard(mutex)(&cp->mutex);

	kmem_slab_t *sp = NULL;
	kmem_bufctl_t *bp = NULL;

	if (cp->destructor != NULL)
		cp->destructor(obj, cp->ctx);

	if (cp->chunksize <= KMEM_SMALL_MAX) {
		// we do +1 as to always land on the end:
		// an object at 0x1000 would align to 0x1000
		sp = (kmem_slab_t *)(ALIGN_UP((uintptr_t)obj + 1, PAGE_SIZE) -
				     sizeof(kmem_slab_t));
		bp = (kmem_bufctl_t *)obj;
	} else {
		kmem_bufctl_t *elm;
		SLIST_FOREACH(elm, &cp->buflist, entry)
		{
			kmem_large_bufctl_t *lbp = (kmem_large_bufctl_t *)elm;
			if (lbp->base == obj) {
				sp = lbp->slab;
				bp = elm;
				break;
			}
		}
	}

	assert(bp != NULL);
	assert(sp != NULL);

	slab_free(sp, bp);
}

kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize, size_t align,
				int (*constructor)(void *, void *),
				void (*destructor)(void *, void *), void *ctx,
				struct vmem *vmp, int kmflags)
{
	size_t chunksize;

	// check if P2
	assert(align == 0 || P2CHECK(align));
	assert(bufsize != 0);

	if (align == 0) {
		if (ALIGN_UP(bufsize, KMEM_ALIGN) >= KMEM_SECOND_ALIGN)
			align = KMEM_SECOND_ALIGN;
		else
			align = KMEM_ALIGN;
	} else if (align < KMEM_ALIGN)
		align = KMEM_ALIGN;

	kmem_cache_t *cp;
	if (kmflags & KMEM_BOOTSTRAP)
		cp = vm_kalloc(ALIGN_UP(sizeof(kmem_cache_t), PAGE_SIZE), 0);
	else
		cp = kmem_cache_alloc(kmem_cache_arena, KM_SLEEP);

	assert(cp);

	strncpy(cp->name, name, KMEM_NAME_MAX);
	cp->bufsize = bufsize;
	cp->align = align;
	cp->constructor = constructor;
	cp->destructor = destructor;
	cp->ctx = ctx;

	cp->vmp = vmp;

	kmutex_init(&cp->mutex, "cp_mutex");

	LIST_INIT(&cp->full_slablist);
	LIST_INIT(&cp->slablist);

	if (bufsize <= KMEM_SMALL_MAX) {
		chunksize = ALIGN_UP(bufsize, align);
		// XXX: determine chunksize for good color
		cp->chunksize = chunksize;
		// store slab meta inline
		cp->slabcap = (PAGE_SIZE - sizeof(kmem_slab_t)) / cp->chunksize;
		cp->slabsize = PAGE_SIZE;
	} else {
		chunksize = cp->chunksize = ALIGN_UP(bufsize, align);
		cp->slabsize = UINTPTR_MAX;

		if (kmflags & KMC_QCACHE) {
			cp->slabsize = next_ilog2(chunksize) * PAGE_SIZE;
			cp->slabcap = cp->slabsize / chunksize;
			return cp;
		}

		size_t pages;
		// XXX: check where min waste
		for (pages = 1; pages < KMEM_MAXPAGES + 1; pages++) {
			// aim for 16 chunks minimum
			if ((pages * PAGE_SIZE / cp->chunksize) >= 16) {
				cp->slabcap =
					(pages * PAGE_SIZE / cp->chunksize);
				cp->slabsize = pages * PAGE_SIZE;
				break;
			}
		}
		assert(cp->slabsize != UINTPTR_MAX);
	}

	return cp;
}

status_t kmem_cache_destroy(kmem_cache_t *cp)
{
	if (!LIST_EMPTY(&cp->full_slablist)) {
		pr_warn("slab(s) not empty: %p\n",
			LIST_FIRST(&cp->full_slablist));
		return YAK_BUSY;
	}

	while (!LIST_EMPTY(&cp->slablist)) {
		kmem_slab_t *sp = LIST_FIRST(&cp->slablist);
		if (sp->refcnt != 0)
			return -1;
		LIST_REMOVE(sp, entry);
		if (sp->base == NULL) {
			vm_kfree((void *)ALIGN_DOWN((uintptr_t)sp, PAGE_SIZE),
				 cp->slabsize);
		} else {
			while (!SLIST_EMPTY(&sp->freelist)) {
				kmem_bufctl_t *elm = SLIST_FIRST(&sp->freelist);
				SLIST_REMOVE_HEAD(&sp->freelist, entry);
				kmem_cache_free(kmem_bufctl_arena, elm);
			}

			vm_kfree(sp->base, cp->slabsize);
			kmem_cache_free(kmem_slab_arena, sp);
		}
	}

	vm_kfree(cp, DIV_ROUNDUP(sizeof(kmem_cache_t), PAGE_SIZE));

	return YAK_SUCCESS;
}

void kmem_init()
{
	kmem_cache_arena = kmem_cache_create("kmem_cache", sizeof(kmem_cache_t),
					     0, NULL, NULL, NULL, NULL,
					     KMEM_BOOTSTRAP);

	kmem_slab_arena = kmem_cache_create("kmem_slab", sizeof(kmem_slab_t), 0,
					    NULL, NULL, NULL, NULL, 0);

	kmem_bufctl_arena = kmem_cache_create("kmem_bufctl",
					      sizeof(kmem_large_bufctl_t), 0,
					      NULL, NULL, NULL, NULL, 0);
}

void kmem_slab_cleanup()
{
	kmem_cache_destroy(kmem_cache_arena);
	kmem_cache_destroy(kmem_slab_arena);
	kmem_cache_destroy(kmem_bufctl_arena);
}
