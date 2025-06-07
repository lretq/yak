#pragma once

#include <stdint.h>
#include <yak/arch-mm.h>

struct pmap {
	uintptr_t top_level;
};

void pmap_kernel_bootstrap(struct pmap *pmap);

void pmap_map(struct pmap *pmap, uintptr_t va, uintptr_t pa, size_t level,
	      vm_prot_t prot, vm_cache_t cache);

void pmap_unmap(struct pmap *pmap, uintptr_t va, size_t level);

void pmap_unmap_range(struct pmap *pmap, uintptr_t va, size_t length,
		      size_t level);

void pmap_activate(struct pmap *pmap);

void pmap_large_map_range(struct pmap *pmap, uintptr_t base, size_t length,
			  uintptr_t virtual_base, vm_prot_t prot,
			  vm_cache_t cache);
