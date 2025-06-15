#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <yak/vmflags.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define PMAP_MAX_LEVELS 5
extern size_t PMAP_LEVELS;

#define BUDDY_ORDERS 20

#define KMAP_ARENA_BASE 0xFFFFFFFe00000000

extern size_t HHDM_BASE;

static size_t PMAP_LEVEL_SHIFTS[] = { 12, 21, 30, 39, 48 };
static size_t PMAP_LEVEL_BITS[] = { 9, 9, 9, 9, 9 };
static size_t PMAP_LEVEL_ENTRIES[] = { 512, 512, 512, 512, 512 };

#define PMAP_HAS_LARGE_PAGE_SIZES 1
static size_t PMAP_LARGE_PAGE_SIZES[] = { 2097152, 1073741824 };

typedef uint64_t pte_t;

enum {
	VM_UC = 0,
	VM_WC,
	VM_WT,
	VM_WB,
	VM_CACHE_DEFAULT = VM_WB,
	VM_CACHE_DISABLE = VM_UC,
};

enum pte_masks {
	ptePresent = 0x1,
	pteWrite = 0x2,
	pteUser = 0x4,
	ptePwt = 0x8,
	ptePcd = 0x10,
	pteAccess = 0x20,
	pteModified = 0x40,
	ptePat = 0x80,
	ptePagesize = 0x80,
	pteGlobal = 0x100,
	ptePatLarge = (1 << 12),
	pteAddress = 0x000FFFFFFFFFF000,
	pteLargeAddress = 0x000FFFFFFFFFe000,
	pteNoExecute = 0x8000000000000000,
};

static inline int pte_is_zero(pte_t pte)
{
	return pte == 0;
}

static inline int pte_is_large(pte_t pte, size_t lvl)
{
	return (lvl > 0 && (pte & ptePagesize) != 0);
}

static inline uintptr_t pte_paddr(pte_t pte)
{
	return pte & pteAddress;
}

static inline pte_t pte_make_dir(uintptr_t pa)
{
	return ptePresent | pteWrite | pteUser | pa;
}

static inline uintptr_t pte_pat_bits(size_t level, vm_cache_t cache)
{
	uintptr_t bits = 0;

	switch (cache) {
	case VM_WC:
		if (level > 0)
			bits = ptePatLarge;
		else
			bits = ptePat;
		bits |= ptePwt;
		break;
	case VM_WT:
		bits = ptePwt;
		break;
	case VM_WB:
		break;
	case VM_UC:
		bits = ptePwt | ptePwt;
		break;
	default:
		bits = ptePcd | ptePwt;
	}

	return bits;
}

static inline pte_t pte_make(size_t level, uintptr_t pa, vm_prot_t prot,
			     vm_cache_t cache)
{
	pte_t pte = ptePresent;

	if (prot & VM_WRITE)
		pte |= pteWrite;

	if (!(prot & VM_EXECUTE))
		pte |= pteNoExecute;

	if (prot & VM_USER)
		pte |= pteUser;

	if (prot & VM_GLOBAL)
		pte |= pteGlobal;

	if (level > 0)
		pte |= ptePagesize;

	pte |= pte_pat_bits(level, cache);

	pte |= pa;

	return pte;
}

#ifdef __cplusplus
}
#endif
