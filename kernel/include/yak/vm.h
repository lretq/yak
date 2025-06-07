#pragma once

#include <stdint.h>
#include <yak/status.h>
#include <yak/arch-mm.h>

static inline uintptr_t p2v(uintptr_t p)
{
	return p + HHDM_BASE;
}

static inline uintptr_t v2p(uintptr_t v)
{
	return v - HHDM_BASE;
}

struct vm_map;

enum {
	VM_FAULT_USER = (1 << 0),
	VM_FAULT_READ = (1 << 1),
	VM_FAULT_WRITE = (1 << 2),
};
status_t vm_handle_fault(struct vm_map *map, uintptr_t address,
			 unsigned long fault_flags);
