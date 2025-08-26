#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/mutex.h>
#include <yak/status.h>
#include <yak/tree.h>
#include <yak/queue.h>

SLIST_HEAD(vspace_freelist, vspace_node);
RB_HEAD(vspace_tree, vspace_node);
RB_PROTOTYPE(vspace_tree, vspace_node, tree_entry, vspace_node_cmp);

struct vspace {
	struct kmutex lock;

	struct vspace_tree free_tree; /* tree of free memory areas */
	struct vspace_tree alloc_tree; /* tree of allocated memory areas */

	struct kmutex freelist_lock;
	struct vspace_freelist freelist; /* free nodes avail. for use */
	size_t nodes_avail; /* no. of nodes on freelist */
};

status_t vspace_xalloc(struct vspace *vs, size_t request, size_t align,
		       int exact, vaddr_t *out);

status_t vspace_alloc(struct vspace *vs, size_t request, size_t align,
		      vaddr_t *out);

void vspace_free(struct vspace *vs, vaddr_t va, size_t size);

void vspace_add_range(struct vspace *vs, vaddr_t start, size_t length);

void vspace_init(struct vspace *vs);

void vspace_dump(struct vspace *vs);

extern struct vspace kernel_va;

#ifdef __cplusplus
}
#endif
