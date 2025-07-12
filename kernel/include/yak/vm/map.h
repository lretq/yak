#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <yak/vm/pmap.h>
#include <yak/rwlock.h>
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

	/*!
	 * Retrieve pages from backing store
	 *
	 * @retval YAK_SUCCESS on success
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

	// put page(s) into backing store
	uintptr_t (*pa_put)(struct vm_object *object, struct page **pages,
			    voff_t offset);
};

struct vm_anon {
	// anon stuff
	struct page *page;
	voff_t offset;
	size_t refcnt;
};

struct vm_amap_l1 {
	struct vm_anon *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap_l2 {
	struct vm_amap_l1 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap {
	struct vm_amap_l2 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_object {
	struct kmutex obj_lock;
	struct vm_pagerops *pg_ops;
	size_t refcnt;
};

enum {
	VM_MAP_ENT_MMIO = 1,
	VM_MAP_ENT_OBJ,
};

struct vm_map_entry {
	vaddr_t base; /*! start address */
	vaddr_t end; /*! end address exclusive */

	unsigned short type; /*! mapping type;
				 valid are mmio & obj */

	union {
		paddr_t mmio_addr; /*! backing physical device memory */
		struct vm_object *object; /*! backing object */
	};
	voff_t offset; /*! offset into backing store */

	struct vm_amap *amap; /*! reference to the amap */

	vm_prot_t protection; /*! current protection */
	vm_prot_t max_protection; /*! maximum protection */

	vm_inheritance_t inheritance; /*! inheritance mode */

	vm_cache_t cache; /*! cache mode */

	RBT_ENTRY(struct vm_map_entry) tree_entry;
};

struct vm_map {
	struct rwlock map_lock;
	RBT_HEAD(vm_map_rbtree, struct vm_map_entry) map_tree;

	struct pmap pmap;
};

/// @brief Retrieve the global kernel VM map
struct vm_map *kmap();

/*!
 * @brief Initialize a VM map
 * 
 * @param map Target VM map
 * 
 * @retval YAK_SUCCESS on success
 */
status_t vm_map_init(struct vm_map *map);

/*!
 * @brief Allocate virtual address from the map arena
 *
 * @param length Space to reserve in bytes
 *
 * @param[out] out Receives the allocated virtual address
 *
 * @retval YAK_SUCCESS on success
 */
status_t vm_map_alloc(struct vm_map *map, size_t length, vaddr_t *out);

/*!
 * @brief Setup a MMIO mapping
 *
 * Essentially, a lazy version of @pmap_map_range, that handles mapping
 * physical addresses which may not be page-aligned
 *
 * @param map Target VM map
 * @param device_addr Physical address (may be unaligned)
 * @param length Length of the mapping in bytes 
 * @param prot Memory protection flags
 * @param cache Cache behaviour (Arch-defined; default is always VM_CACHE_DEFAULT)
 * @param[out] out On success, receives the vaddr of the mapped region
 */
status_t vm_map_mmio(struct vm_map *map, paddr_t device_addr, size_t length,
		     vm_prot_t prot, vm_cache_t cache, vaddr_t *out);

/**
 * @brief Remove a MMIO mapping, also removes physical memory mapping
 *
 * @param map Target VM map
 * @param va mapped address, offset does NOT need to be removed
 */
status_t vm_unmap_mmio(struct vm_map *map, vaddr_t va);

/*!
 * @brief Map a VM object or a zero-fill anon space
 *
 * @param map Target VM map
 *
 * @param obj Nullable pointer to a VM object (Purely anonymous mapping if NULL)
 *
 * @param length Length of the mapping in bytes
 *
 * @param offset Offset into the object in bytes
 *
 * @param map_exact If set, try to use address in out or fail
 *
 * @param initial_prot Memory protection flags
 *
 * @param inheritance Mapping inheritance on map clone
 *
 * @param[in,out] out May contain a hint for address allocation; 
 *                If map_exact is set, must contain a valid address. 
 *                On return, receives the address of the mapped region
 *
 * @retval YAK_SUCCESS on success
 */
status_t vm_map(struct vm_map *map, struct vm_object *obj, size_t length,
		voff_t offset, int map_exact, vm_prot_t initial_prot,
		vm_inheritance_t inheritance, vaddr_t *out);

/*!
 * @brief Remove any (page-aligned) VM mapping
 *
 * @param map Target VM map
 * @param va Mapping address
 */
status_t vm_unmap(struct vm_map *map, vaddr_t va);

#ifdef __YAK_PRIVATE__
void vm_object_common_init(struct vm_object *obj, struct vm_pagerops *pgops);

struct vm_amap *vm_amap_create();
void vm_amap_destroy(struct vm_amap *amap);

void vm_map_activate(struct vm_map *map);

struct vm_map_entry *vm_map_lookup_entry_locked(struct vm_map *map,
						vaddr_t address);
#endif

#ifdef __cplusplus
}
#endif
