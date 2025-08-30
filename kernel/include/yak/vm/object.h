#pragma once

#include <yak/status.h>
#include <yak/types.h>
#include <yak/mutex.h>
#include <yak/vm/page.h>

struct vm_object;

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

	// makes additional bookkeeping possible
	// like inc'ing vnode reference??
	void (*pgo_ref)(struct vm_object *obj);

	// called when refcnt=0, before destroying the object
	// should write back all pages
	void (*pgo_cleanup)(struct vm_object *obj);
};

struct vm_object {
	struct kmutex obj_lock;
	struct vm_pagerops *pg_ops;
	struct vm_page_tree memq;
	size_t refcnt;
};

void vm_object_retain(struct vm_object *obj);
void vm_object_release(struct vm_object *obj);

void vm_object_common_init(struct vm_object *obj, struct vm_pagerops *pgops);

status_t vm_lookuppage(struct vm_object *obj, voff_t offset, int flags,
		       struct page **pagep);
