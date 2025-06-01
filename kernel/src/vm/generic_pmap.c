#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <yak/panic.h>
#include <yak/log.h>
#include <yak/arch-mm.h>
#include <yak/arch-cpu.h>
#include <yak/macro.h>
#include <yak/vm.h>
#include <yak/vm/pmap.h>
#include <yak/vm/pmm.h>
#include <yak/vm/page.h>

#define PTE_LOAD(p) (__atomic_load_n((p), __ATOMIC_SEQ_CST))
#define PTE_STORE(p, x) (__atomic_store_n((p), (x), __ATOMIC_SEQ_CST))

static pte_t *pte_fetch(struct pmap *pmap, uintptr_t va, size_t atLevel,
			int alloc)
{
	pte_t *table = (pte_t *)p2v(pmap->top_level);

	size_t indexes[PMAP_MAX_LEVELS];

	for (size_t i = 0; i < PMAP_LEVELS; i++) {
		indexes[i] = (va >> PMAP_LEVEL_SHIFTS[i]) &
			     ((1ULL << PMAP_LEVEL_BITS[i]) - 1);
	}

	for (size_t lvl = PMAP_LEVELS - 1;; lvl--) {
		assert(lvl >= 0 && lvl <= PMAP_LEVELS - 1);
		pte_t *ptep = &table[indexes[lvl]];
		assert((uint64_t)ptep >= 0xffff800000000000);
		pte_t pte = PTE_LOAD(ptep);

		if (atLevel == lvl) {
			return ptep;
		}

		if (pte_is_zero(pte)) {
			if (!alloc) {
				return NULL;
			}

			struct page *page = pmm_alloc_order(0);
			assert(page);

			uintptr_t pa = page_to_addr(page);

			pte = pte_make_dir(pa);
			PTE_STORE(ptep, pte);
		} else {
			assert(!pte_is_large(pte, lvl));
		}

		table = (uint64_t *)p2v(pte_paddr(pte));
	}

	return NULL;
}

void pmap_kernel_bootstrap(struct pmap *pmap)
{
	pmap->top_level = pmm_alloc();
	uint64_t *top_dir = (uint64_t *)p2v(pmap->top_level);
	// preallocate the top half so we can share among user maps
	for (size_t i = PMAP_LEVEL_ENTRIES[PMAP_LEVELS] / 2;
	     i < PMAP_LEVEL_ENTRIES[PMAP_LEVELS]; i++) {
		if (pte_is_zero(top_dir[i])) {
			top_dir[i] = pte_make_dir(pmm_alloc());
		}
	}
}

void pmap_map(struct pmap *pmap, uintptr_t va, uintptr_t pa, size_t level,
	      vm_prot_t prot, vm_cache_t cache)
{
	assert(prot & VM_READ);

	pte_t *ppte = pte_fetch(pmap, va, level, 1);
	pte_t pte = PTE_LOAD(ppte);

	PTE_STORE(ppte, pte_make(level, pa, prot, cache));

	if (!pte_is_zero(pte)) {
		// TODO: TLB shootdown, check if permissions changed to be less permissive
		asm volatile("invlpg %0" ::"m"(va) : "memory");
	}
}

void pmap_unmap(struct pmap *pmap, uintptr_t va, size_t level)
{
	pte_t *ppte = pte_fetch(pmap, va, level, 0);
	if (ppte) {
		PTE_STORE(ppte, 0);
		asm volatile("invlpg %0" ::"m"(va) : "memory");
	}
}

void pmap_large_map_range(struct pmap *pmap, uintptr_t base, size_t length,
			  uintptr_t virtual_base, vm_prot_t prot,
			  vm_cache_t cache)
{
	uintptr_t end = base + length;
	uintptr_t curr = base;

	// map until aligned
	while (curr < end) {
#ifdef PMAP_HAS_LARGE_PAGE_SIZES
		if (IS_ALIGNED_POW2(curr, PMAP_LARGE_PAGE_SIZES[0])) {
			break;
		}
#endif
		pmap_map(pmap, virtual_base + curr - base, curr, 0, prot,
			 cache);
		curr += PAGE_SIZE;
	}

#ifdef PMAP_HAS_LARGE_PAGE_SIZES
	for (size_t i = elementsof(PMAP_LARGE_PAGE_SIZES); i > 0; i--) {
		while (curr < end) {
			if (curr + PMAP_LARGE_PAGE_SIZES[i - 1] >= end ||
			    !IS_ALIGNED_POW2(curr,
					     PMAP_LARGE_PAGE_SIZES[i - 1])) {
				goto outer;
			}

			pmap_map(pmap, virtual_base - base + curr, curr, i,
				 prot, cache);

			curr += PMAP_LARGE_PAGE_SIZES[i - 1];
		}
outer:;
	}

	// map until end
	while (curr < end) {
		pmap_map(pmap, virtual_base - base + curr, curr, 0, prot,
			 cache);
		curr += PAGE_SIZE;
	}
#endif

	assert(curr == end);
}
