#define pr_fmt(fmt) "pmm: " fmt

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <yak/queue.h>
#include <yak/log.h>
#include <yak/spinlock.h>
#include <yak/panic.h>
#include <yak/macro.h>
#include <yak/vm/pmm.h>
#include <yak/arch-mm.h>
#include <yak/vm/page.h>
#include <yak/vm.h>

struct region {
	paddr_t base, end;
	SLIST_ENTRY(region) list_entry;
	struct page pages[];
};

static SLIST_HEAD(region_list,
		  region) region_list = SLIST_HEAD_INITIALIZER(region_list);

static struct region *lookup_region(paddr_t addr)
{
	struct region *region;
	SLIST_FOREACH(region, &region_list, list_entry)
	{
		if (addr >= region->base && addr <= region->end)
			return region;
	}

	return NULL;
}

struct page *pmm_lookup_page(paddr_t addr)
{
	struct region *region = lookup_region(addr);
	if (!region)
		return NULL;

	const paddr_t base_pfn = (region->base >> PAGE_SHIFT);

	return &region->pages[(addr >> PAGE_SHIFT) - base_pfn];
}

typedef TAILQ_HEAD(page_list, page) page_list_t;

struct zone {
	int zone_id;
	const char *zone_name;
	paddr_t base, end;
	struct spinlock zone_lock;
	int may_alloc;

	SLIST_ENTRY(zone) list_entry;

	page_list_t orders[BUDDY_ORDERS];

	size_t npages[BUDDY_ORDERS];
};

static SLIST_HEAD(zone_list, zone) zone_list;

static size_t total_pagecnt = 0;
static size_t usable_pagecnt = 0;

#define MAX_ZONES 8
static struct zone static_zones[MAX_ZONES];
static size_t static_zones_pos = 0;

void pmm_zone_init(int zone_id, const char *name, int may_alloc, paddr_t base,
		   paddr_t end)
{
	size_t pos = __atomic_fetch_add(&static_zones_pos, 1, __ATOMIC_SEQ_CST);

	if (pos >= MAX_ZONES)
		panic("tried to initialize too many zones");

	struct zone *zone = &static_zones[pos];

	zone->zone_id = zone_id;
	zone->zone_name = name;

	zone->base = base;
	zone->end = end;

	spinlock_init(&zone->zone_lock);

	zone->may_alloc = may_alloc;

	for (int i = 0; i < BUDDY_ORDERS; i++) {
		TAILQ_INIT(&zone->orders[i]);
		zone->npages[i] = 0;
	}

	if (SLIST_EMPTY(&zone_list)) {
		SLIST_INSERT_HEAD(&zone_list, zone, list_entry);
		return;
	}

	// insert zones sorted by address
	struct zone *ent, *min_zone = NULL;
	SLIST_FOREACH(ent, &zone_list, list_entry)
	{
		if (ent->base > zone->base)
			break;
		min_zone = ent;
	}

	if (min_zone == NULL) {
		SLIST_INSERT_HEAD(&zone_list, zone, list_entry);
		return;
	}

	pr_info("min_zone->base=0x%lx zone->base=0x%lx\n", min_zone->base,
		zone->base);

	SLIST_INSERT_AFTER(min_zone, zone, list_entry);
}

static struct zone *lookup_zone(paddr_t addr)
{
	struct zone *ent;
	SLIST_FOREACH(ent, &zone_list, list_entry)
	{
		if (addr >= ent->base && addr < ent->end)
			return ent;
	}
	return NULL;
}

static struct zone *lookup_zone_by_id(int zone_id)
{
	struct zone *ent;
	SLIST_FOREACH(ent, &zone_list, list_entry)
	{
		if (ent->zone_id == zone_id)
			return ent;
	}
	return NULL;
}

void page_zero(struct page *page, unsigned int order)
{
	memset((void *)page_to_mapped_addr(page), 0,
	       (1ULL << (order + PAGE_SHIFT)));
}

void pmm_init()
{
	pmm_zone_init(ZONE_HIGH, "ZONE_HIGH", 1, UINT32_MAX, UINT64_MAX);
	pmm_zone_init(ZONE_LOW, "ZONE_LOW", 1, 1048576, UINT32_MAX);
	pmm_zone_init(ZONE_1MB, "ZONE_1MB", 0, 0x0, 1048576);
}

void pmm_add_region(paddr_t base, paddr_t end)
{
	assert((base % PAGE_SIZE) == 0);
	assert((end % PAGE_SIZE) == 0);

	assert(((base > UINT32_MAX) && (end > UINT32_MAX)) ||
	       ((base < UINT32_MAX) && (end < UINT32_MAX)));

	struct zone *zone = lookup_zone(base);

	size_t pagecnt_total = (end - base) >> PAGE_SHIFT;
	size_t pagecnt_used =
		(ALIGN_UP(sizeof(struct region) +
				  sizeof(struct page) * pagecnt_total,
			  PAGE_SIZE)) >>
		PAGE_SHIFT;

	struct region *desc = (struct region *)p2v(base);

	desc->base = base;
	desc->end = end;

	if (SLIST_EMPTY(&region_list)) {
		SLIST_INSERT_HEAD(&region_list, desc, list_entry);
	} else {
		struct region *ent, *min_region;
		SLIST_FOREACH(ent, &region_list, list_entry)
		{
			if (ent->base > desc->base)
				break;
			min_region = ent;
		}

		if (min_region == NULL) {
			SLIST_INSERT_HEAD(&region_list, desc, list_entry);
		} else {
			SLIST_INSERT_AFTER(min_region, desc, list_entry);
		}
	}

	total_pagecnt += pagecnt_total;
	usable_pagecnt += pagecnt_total - pagecnt_used;

	memset(desc->pages, 0, sizeof(struct page) * pagecnt_total);

	const paddr_t base_pfn = base >> PAGE_SHIFT;
	for (size_t i = 0; i < pagecnt_used; i++) {
		struct page *page = &desc->pages[i];
		page->pfn = base_pfn + i;
		page->shares = 1;
		page->max_order = -1;
	}

	if (pagecnt_total - pagecnt_used <= 0)
		return;

	for (size_t i = base + pagecnt_used * PAGE_SIZE; i < end;) {
		unsigned int max_order = 0;
		while ((max_order < BUDDY_ORDERS - 1) &&
		       (i + (2ULL << (PAGE_SHIFT + max_order)) < end) &&
		       IS_ALIGNED_POW2(i, (2ULL << (PAGE_SHIFT + max_order)))) {
			max_order++;
		}

		const size_t blksize = 1ULL << (PAGE_SHIFT + max_order);

		for (size_t j = i; j < i + blksize; j += PAGE_SIZE) {
			const paddr_t pfn = (j >> PAGE_SHIFT);
			struct page *page = &desc->pages[pfn - base_pfn];
			page->pfn = pfn;
			page->shares = 0;
			page->max_order = max_order;
		}

		const paddr_t pfn = (i >> PAGE_SHIFT);
		struct page *page = &desc->pages[pfn - base_pfn];

		TAILQ_INSERT_TAIL(&zone->orders[max_order], page, tailq_entry);
		zone->npages[max_order] += 1;

		i += blksize;
	}

	pr_debug("added 0x%lx-0x%lx\n", base, end);
}

static struct page *zone_alloc(struct zone *zone, unsigned int order)
{
	assert(order < BUDDY_ORDERS);

	ipl_t ipl = spinlock_lock(&zone->zone_lock);

	if (zone->npages[order] > 0) {
		struct page *page = TAILQ_FIRST(&zone->orders[order]);
		// -> npages >= 1
		assert(page);
		TAILQ_REMOVE(&zone->orders[order], page, tailq_entry);
		zone->npages[order] -= 1;

		// check if page is actually free
		assert(page->shares == 0);

		page->shares = 1;

		spinlock_unlock(&zone->zone_lock, ipl);

		return page;
	}

	size_t next_order = order + 1;
	while (next_order < BUDDY_ORDERS && zone->npages[next_order] == 0) {
		next_order += 1;
	}

	if (next_order >= BUDDY_ORDERS) {
		spinlock_unlock(&zone->zone_lock, ipl);
		return NULL;
	}

	size_t i = next_order;
	struct page *buddy_page = TAILQ_FIRST(&zone->orders[i]);
	TAILQ_REMOVE(&zone->orders[i], buddy_page, tailq_entry);
	zone->npages[i] -= 1;

	while (i > order) {
		i -= 1;

		TAILQ_INSERT_TAIL(&zone->orders[i], buddy_page, tailq_entry);
		zone->npages[i] += 1;

		const size_t blksize = (1ULL << (PAGE_SHIFT + i));

		const paddr_t buddy_addr = page_to_addr(buddy_page) ^ blksize;

		buddy_page = pmm_lookup_page(buddy_addr);
	}

	assert(buddy_page->shares == 0);
	buddy_page->shares = 1;

	spinlock_unlock(&zone->zone_lock, ipl);

	return buddy_page;
}

static void zone_free(struct zone *zone, struct page *page, unsigned int order)
{
	assert(page);
	assert(order < BUDDY_ORDERS);

	ipl_t ipl = spinlock_lock(&zone->zone_lock);

	while (order < page->max_order) {
		const size_t blksize = (1ULL << (PAGE_SHIFT + order));

		paddr_t addr = page_to_addr(page);
		paddr_t buddy_addr = addr ^ blksize;
		struct page *buddy_page = pmm_lookup_page(buddy_addr);
		assert(buddy_page);
		if (buddy_page->shares != 0 ||
		    buddy_page->max_order != page->max_order)
			break;

		assert(zone->npages[order] > 0);
		TAILQ_REMOVE(&zone->orders[order], buddy_page, tailq_entry);
		zone->npages[order] -= 1;

		page->shares = 0;

		order += 1;
		page = (struct page *)p2v((addr & ~(1ULL << order)));
	}

	page->shares = 0;
	TAILQ_INSERT_TAIL(&zone->orders[order], page, tailq_entry);
	zone->npages[order] += 1;

	spinlock_unlock(&zone->zone_lock, ipl);
}

struct page *pmm_alloc_order(unsigned int order)
{
	struct page *page;
	struct zone *zone;
	SLIST_FOREACH(zone, &zone_list, list_entry)
	{
		if (!zone->may_alloc)
			continue;
		page = zone_alloc(zone, order);
		if (page != NULL)
			return page;
	}
	return NULL;
}

struct page *pmm_zone_alloc_order(int zone_id, unsigned int order)
{
	struct zone *zone = lookup_zone_by_id(zone_id);
	if (!zone)
		return NULL;
	return zone_alloc(zone, order);
}

void pmm_free_order(paddr_t addr, unsigned int order)
{
	struct page *page = pmm_lookup_page(addr);
	zone_free(lookup_zone(addr), page, order);
}

void pmm_free_pages_order(struct page *page, unsigned int order)
{
	zone_free(lookup_zone(page_to_addr(page)), page, order);
}

void pmm_dump()
{
	printk(0, "\n=== PMM DUMP ===\n");

	struct zone *zone;
	SLIST_FOREACH(zone, &zone_list, list_entry)
	{
		int empty = 1;
		for (int i = 0; i < BUDDY_ORDERS; i++) {
			if (zone->npages[i] > 0) {
				empty = 0;
				break;
			}
		}
		if (empty)
			continue;

		printk(0, "\n%s:\n", zone->zone_name);

		for (int i = 0; i < BUDDY_ORDERS; i++) {
			if ((zone)->npages[i] > 0)
				printk(0, " * order %d pagecount: %ld\n", i,
				       (zone)->npages[i]);
		}
	}

	printk(0, "\nusable memory: %zuMiB/%zuMiB\n", usable_pagecnt >> 8,
	       total_pagecnt >> 8);

	printk(0, "\n");
}
