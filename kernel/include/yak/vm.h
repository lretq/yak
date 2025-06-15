#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/status.h>
#include <yak/arch-mm.h>
#include <yak/types.h>

static inline vaddr_t p2v(paddr_t p)
{
	return p + HHDM_BASE;
}

static inline paddr_t v2p(vaddr_t v)
{
	return v - HHDM_BASE;
}

struct vm_map;

enum {
	VM_FAULT_USER = (1 << 0),
	VM_FAULT_READ = (1 << 1),
	VM_FAULT_WRITE = (1 << 2),
};

status_t vm_handle_fault(struct vm_map *map, vaddr_t address,
			 unsigned long fault_flags);

#ifdef __cplusplus
}
#endif
