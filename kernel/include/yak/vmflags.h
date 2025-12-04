#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define VM_SLEEP 0x1
#define VM_NOSLEEP 0x2

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

enum {
	VM_MAP_FIXED = 0x1,
	VM_MAP_OVERWRITE = 0x2,
	VM_MAP_PREFILL = 0x4,
	VM_MAP_LOCK_HELD = 0x8,
};

typedef enum {
	VM_INHERIT_NONE = 0,
	VM_INHERIT_SHARED,
	VM_INHERIT_COPY,
} vm_inheritance_t;

typedef uint32_t vm_prot_t;
typedef unsigned int vm_cache_t;

#ifdef __cplusplus
}
#endif
