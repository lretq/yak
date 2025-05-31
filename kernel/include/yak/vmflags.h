#pragma once

#include <stdint.h>

enum {
	VM_READ = (1 << 0),
	VM_WRITE = (1 << 1),
	VM_EXECUTE = (1 << 2),
	VM_USER = (1 << 3),
	VM_GLOBAL = (1 << 4),
	VM_HUGE = (1 << 5),

	// huge size in log2 in last 6 bits of 32 bits
	VM_HUGE_SHIFT = 26,
	VM_HUGE_2MB = (20 << VM_HUGE_SHIFT),
	VM_HUGE_1GB = (29 << VM_HUGE_SHIFT),

	// for convenience
	VM_RW = VM_READ | VM_WRITE,
	VM_RX = VM_READ | VM_EXECUTE,
};

typedef uint32_t vm_prot_t;
typedef unsigned int vm_cache_t;
