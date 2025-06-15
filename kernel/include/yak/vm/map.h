#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <yak/vm/pmap.h>
#include <yak/status.h>
#include <yak/tree.h>
#include <yak/vmflags.h>
#include <yak/types.h>
#include <yak/mutex.h>

struct vm_object;
struct page;

/*! 
 * @brief VM Pager operations 
 *
 * Interface between the VM and the pager's backing store
 */
struct vm_pagerops {
	/*! Pager name */
	const char *pgo_name;

	/*! Init private pager data structures, run once at boot */
	void (*pgo_init)();

	void (*pgo_reference)();
	void (*pgo_detach)();

	/*!
	 * Retrieve pages from backing store
	 *
	 * @returns YAK_SUCCESS if ok, else TODO
	 *
	 * @param[in] obj The backing memory object
	 *
	 * @param[in] offset Offset into the object (aligned to page boundary)
	 *
	 * @param[in,out] pages Array of page pointers where the fetched pages should be stored, the pager fills this in
	 *
	 * @param[in,out] npages On input, represents the number of pages that may be read clustered. On output, represents the actual number of pages read.
	 *
	 * @param[in] centeridx Index in pages[] for the faulting page
	 *
	 * @param[in] access_type The type of access that caused the fault
	 *
	 * @param[in] flags Additional control flags
	 */
	status_t (*pgo_get)(struct vm_object *obj, voff_t offset,
			    struct page **pages, unsigned int *npages,
			    unsigned int centeridx, vm_prot_t access_type,
			    unsigned int flags);

	uintptr_t (*pa_put)(struct vm_object *object, struct page **pages,
			    voff_t offset);
};

struct vm_object {
	struct vm_pagerops *ops;
};

struct vm_anon {
	// =1 ref = writable
	// >1 refs = CoW
	size_t refcount;
};

enum {
	VM_MAP_ENT_MMIO = 1,
	VM_MAP_ENT_OBJ,
	VM_MAP_ENT_ANON,
};

struct vm_map_entry {
	RBT_ENTRY(struct vm_map_entry) tree_entry;

	vaddr_t base; /* start address */
	vaddr_t end; /* end address exclusive */

	unsigned short type;

	union {
		paddr_t mmio_addr; /* backing physical device memory */
		struct vm_object *object; /* backing object */
	};
	voff_t offset; /* offset into backing store */

	vm_prot_t protection;
	vm_prot_t max_protection;

	vm_inheritance_t inheritance;

	vm_cache_t cache;
};

struct vm_map {
	RBT_HEAD(vm_map_rbtree, struct vm_map_entry) map_tree;
	struct kmutex lock;

	struct pmap pmap;
};

struct vm_map *kmap();

status_t vm_map_init(struct vm_map *map);

status_t vm_map_alloc(struct vm_map *map, size_t length, vaddr_t *out);

struct vm_map_entry *vm_map_lookup_entry_locked(struct vm_map *map,
						vaddr_t address);

// setup MMIO mapping
status_t vm_map_mmio(struct vm_map *map, paddr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, vaddr_t *out);
status_t vm_unmap_mmio(struct vm_map *map, vaddr_t va);

status_t vm_map(struct vm_map *map, struct vm_object *obj,
		vm_prot_t initial_prot, vm_inheritance_t inheritance,
		vaddr_t *out);

status_t vm_unmap(struct vm_map *map, vaddr_t va);

#ifdef __cplusplus
}
#endif
