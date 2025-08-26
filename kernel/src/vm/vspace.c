#define pr_fmt(fmt) "vspace: " fmt

#include <assert.h>
#include <yak/arch-mm.h>
#include <yak/log.h>
#include <yak/macro.h>
#include <yak/mutex.h>
#include <yak/sched.h>
#include <yak/status.h>
#include <yak/vm/pmm.h>
#include <yak/types.h>
#include <yak/tree.h>
#include <yak/queue.h>
#include <yak/hint.h>
#include <yak/vm/vspace.h>
#include <yak/cleanup.h>

struct vspace_node {
	/* a vspace_node may be either in-tree or in a vspace freelist on standby */
	union {
		RB_ENTRY(vspace_node) tree_entry;
		SLIST_ENTRY(vspace_node) freelist_entry;
	};

	vaddr_t start; /* inclusive */
	vaddr_t end; /* exclusive */
};

static void vspace_node_init(struct vspace_node *node, vaddr_t start,
			     vaddr_t end)
{
	node->start = start;
	node->end = end;
}

static int vspace_node_cmp(struct vspace_node *a, struct vspace_node *b)
{
	if (a->start < b->start)
		return -1;
	else if (a->start > b->start)
		return 1;
	return 0;
}

RB_GENERATE(vspace_tree, vspace_node, tree_entry, vspace_node_cmp);

static struct vspace_node *vs_get_node(struct vspace *vs)
{
	guard(mutex)(&vs->freelist_lock);

	struct vspace_node *node = SLIST_FIRST(&vs->freelist);
	if (likely(node)) {
		SLIST_REMOVE_HEAD(&vs->freelist, freelist_entry);
		return node;
	}

	// for simplicity: refill with a whole page of nodes
	vaddr_t pg = p2v(pmm_alloc());
	node = (struct vspace_node *)pg;
	// skip the first node
	for (size_t i = 1; i < PAGE_SIZE / sizeof(struct vspace_node); i++) {
		SLIST_INSERT_HEAD(&vs->freelist, &node[i], freelist_entry);
	}

	return node;
}

static void vs_put_node(struct vspace *vs, struct vspace_node *node)
{
	guard(mutex)(&vs->freelist_lock);

	SLIST_INSERT_HEAD(&vs->freelist, node, freelist_entry);
}

// lowest node where >= key->start
static struct vspace_node *tree_lower_bound(struct vspace_tree *tree,
					    struct vspace_node *key)
{
	struct vspace_node *res = NULL;
	struct vspace_node *cur = RB_ROOT(tree);

	while (cur) {
		if (vspace_node_cmp(cur, key) < 0) {
			// too little: have to go right
			cur = RB_RIGHT(cur, tree_entry);
		} else {
			res = cur;
			// but there could be a smaller one still
			cur = RB_LEFT(cur, tree_entry);
		}
	}

	return res;
}

static void vspace_add_range_locked(struct vspace *vs, struct vspace_node *node,
				    vaddr_t start, vaddr_t end)
{
	if (!node)
		node = vs_get_node(vs);
	vspace_node_init(node, start, end);

	struct vspace_node *right = tree_lower_bound(&vs->free_tree, node);
	struct vspace_node *left = right ? RB_PREV(vspace_tree, vs, right) :
					   RB_MAX(vspace_tree, &vs->free_tree);

	// try to merge with left & right neighbours
	if (left && left->end == node->start) {
		RB_REMOVE(vspace_tree, &vs->free_tree, left);
		left->end = node->end;
		vs_put_node(vs, node);
		node = left;
	}

	if (right && node->end == right->start) {
		RB_REMOVE(vspace_tree, &vs->free_tree, right);
		right->start = node->start;
		vs_put_node(vs, node);
		node = right;
	}

	RB_INSERT(vspace_tree, &vs->free_tree, node);
}

void vspace_add_range(struct vspace *vs, vaddr_t start, size_t length)
{
	EXPECT(kmutex_acquire(&vs->lock, TIMEOUT_INFINITE));

	vaddr_t end = start + length;
	vspace_add_range_locked(vs, NULL, start, end);

	kmutex_release(&vs->lock);
}

void vspace_free(struct vspace *vs, vaddr_t va, size_t size)
{
	EXPECT(kmutex_acquire(&vs->lock, TIMEOUT_INFINITE));

	struct vspace_node hint = (struct vspace_node){ .start = va };
	struct vspace_node *node = tree_lower_bound(&vs->alloc_tree, &hint);
	assert(node && node->start == va && node->end == va + size);
	RB_REMOVE(vspace_tree, &vs->alloc_tree, node);

	vspace_add_range_locked(vs, node, va, va + size);

	kmutex_release(&vs->lock);
}

status_t vspace_xalloc(struct vspace *vs, size_t request, size_t align,
		       int exact, vaddr_t *out)
{
	if (unlikely(request == 0 || out == NULL))
		return YAK_INVALID_ARGS;

	assert(IS_ALIGNED_POW2(request, PAGE_SIZE));

	if (align < PAGE_SIZE)
		align = PAGE_SIZE;

	assert(P2CHECK(align));

	EXPECT(kmutex_acquire(&vs->lock, TIMEOUT_INFINITE));

	if (exact) {
		vaddr_t alloc_start = *out;
		vaddr_t alloc_end = alloc_start + request;

		// walk the free tree to see if the requested range is available
		for (struct vspace_node *cur =
			     RB_MIN(vspace_tree, &vs->free_tree);
		     cur != NULL;
		     cur = RB_NEXT(vspace_tree, &vs->free_tree, cur)) {
			if (alloc_start >= cur->start &&
			    alloc_end <= cur->end) {
				RB_REMOVE(vspace_tree, &vs->free_tree, cur);

				const int has_left = (cur->start < alloc_start);
				const int has_right = (alloc_end < cur->end);

				if (has_left && has_right) {
					cur->end = alloc_start;
					RB_INSERT(vspace_tree, &vs->free_tree,
						  cur);

					struct vspace_node *right =
						vs_get_node(vs);
					vspace_node_init(right, alloc_end,
							 cur->end);
					RB_INSERT(vspace_tree, &vs->free_tree,
						  right);
				} else if (has_left) {
					cur->end = alloc_start;
					RB_INSERT(vspace_tree, &vs->free_tree,
						  cur);
				} else if (has_right) {
					cur->start = alloc_end;
					RB_INSERT(vspace_tree, &vs->free_tree,
						  cur);
				} else {
					vs_put_node(vs, cur);
				}

				struct vspace_node *alloc = vs_get_node(vs);
				vspace_node_init(alloc, alloc_start, alloc_end);
				RB_INSERT(vspace_tree, &vs->alloc_tree, alloc);

				*out = alloc_start;
				kmutex_release(&vs->lock);
				return YAK_SUCCESS;
			}
		}

		// exact range not free
		kmutex_release(&vs->lock);
		return YAK_NOSPACE;
	}

	// allocate with first-fit
	for (struct vspace_node *cur = RB_MIN(vspace_tree, &vs->free_tree);
	     cur != NULL; cur = RB_NEXT(vspace_tree, &vs->free_tree, cur)) {
		const vaddr_t old_start = cur->start;
		const vaddr_t old_end = cur->end;

		const vaddr_t alloc_start = ALIGN_UP(old_start, align);

		if (alloc_start >= old_end)
			continue;

		if (alloc_start > old_end - request)
			continue;

		const vaddr_t alloc_end = alloc_start + request;

		assert(IS_ALIGNED_POW2(alloc_start, PAGE_SIZE));
		assert(IS_ALIGNED_POW2(alloc_end, PAGE_SIZE));

		RB_REMOVE(vspace_tree, &vs->free_tree, cur);

		const int has_left = (old_start < alloc_start);
		const int has_right = (alloc_end < old_end);

		if (has_left && has_right) {
			cur->start = old_start;
			cur->end = alloc_start;
			RB_INSERT(vspace_tree, &vs->free_tree, cur);

			struct vspace_node *right = vs_get_node(vs);
			vspace_node_init(right, alloc_end, old_end);
			RB_INSERT(vspace_tree, &vs->free_tree, right);
		} else if (has_left) {
			cur->start = old_start;
			cur->end = alloc_start;
			RB_INSERT(vspace_tree, &vs->free_tree, cur);
		} else if (has_right) {
			cur->start = alloc_end;
			cur->end = old_end;
			RB_INSERT(vspace_tree, &vs->free_tree, cur);
		} else {
			// no remainder
			vs_put_node(vs, cur);
		}

		cur = vs_get_node(vs);
		vspace_node_init(cur, alloc_start, alloc_end);
		RB_INSERT(vspace_tree, &vs->alloc_tree, cur);

		*out = alloc_start;
		kmutex_release(&vs->lock);
		return YAK_SUCCESS;
	}

	kmutex_release(&vs->lock);
	return YAK_OOM;
}

status_t vspace_alloc(struct vspace *vs, size_t request, size_t align,
		      vaddr_t *out)
{
	return vspace_xalloc(vs, request, align, 0, out);
}

void vspace_init(struct vspace *vs)
{
	kmutex_init(&vs->lock, "vspace");
	RB_INIT(&vs->free_tree);
	RB_INIT(&vs->alloc_tree);

	kmutex_init(&vs->freelist_lock, "vspace_freelist");
	SLIST_INIT(&vs->freelist);
	vs->nodes_avail = 0;
}

void vspace_dump(struct vspace *vs)
{
	struct vspace_node *elm;
	pr_debug(" === dump vspace %p ===\n", vs);
	pr_debug("free ranges:\n");
	RB_FOREACH(elm, vspace_tree, &vs->free_tree)
	{
		pr_debug("* [0x%lx - 0x%lx]\n", elm->start, elm->end);
	}
	pr_debug("alloc'd ranges:\n");
	RB_FOREACH(elm, vspace_tree, &vs->alloc_tree)
	{
		pr_debug("* [0x%lx - 0x%lx]\n", elm->start, elm->end);
	}
}
