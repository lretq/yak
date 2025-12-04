#pragma once

#include <yak/mutex.h>
#include <yak/refcount.h>

struct vm_anon {
	/* protects page and offset */
	struct kmutex anon_lock;
	/* either resident page or NULL if swapped out */
	struct page *page;
	/* offset in backing store */
	voff_t offset;
	/* amaps that reference this anon */
	refcount_t refcnt;
};

DECLARE_REFMAINT(vm_anon);

struct vm_anon *vm_anon_create(struct page *page, voff_t offset);

// called with anon lock held
struct vm_anon *vm_anon_copy(struct vm_anon *anon);
