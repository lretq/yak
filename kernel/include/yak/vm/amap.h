#pragma once

#include <yak/types.h>
#include <yak/arch-mm.h>

struct vm_anon {
	/* either resident page or NULL if swapped out */
	struct page *page;
	/* offset in backing store */
	voff_t offset;
	/* amaps that point to this anon */
	size_t refcnt;
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
	struct vm_amap_l3 *l3;
};

struct vm_amap *vm_amap_create();

void vm_amap_retain(struct vm_amap *amap);
void vm_amap_release(struct vm_amap *amap);

struct vm_anon **vm_amap_lookup(struct vm_amap *amap, voff_t offset,
				int create);
