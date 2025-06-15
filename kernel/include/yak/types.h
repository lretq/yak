#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;

typedef size_t voff_t;

// ~600 years
typedef uint64_t nstime_t;

#ifdef __cplusplus
}
#endif
