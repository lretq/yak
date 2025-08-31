#pragma once

#include "yak/refcount.h"
#include <yak/types.h>
#include <yak/arch-mm.h>
#include <yak/vm/object.h>

struct vm_anon {
	/* either resident page or NULL if swapped out */
	struct page *page;
	/* offset in backing store */
	voff_t offset;
	/* amaps that point to this anon */
	refcount_t refcnt;
};

struct vm_amap_l1 {
	struct vm_anon *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap_l2 {
	struct vm_amap_l1 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap_l3 {
	struct vm_amap_l2 *entries[PAGE_SIZE / sizeof(void *)];
};

struct vm_amap {
	size_t refcnt;
	struct vm_object *obj;
	struct vm_amap_l3 *l3;
};

struct vm_amap *vm_amap_create(struct vm_object *obj);

DECLARE_REFMAINT(vm_amap);

struct vm_anon **vm_amap_lookup(struct vm_amap *amap, voff_t offset,
				int create);
