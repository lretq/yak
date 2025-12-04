#pragma once

#include <stddef.h>

typedef struct vmem vmem_t;

#define VM_SLEEP 0x1
#define VM_NOSLEEP 0x2

#define VM_BESTFIT 0x8
#define VM_INSTANTFIT 0x10

#define VM_POPULATE 0x4

#define VM_EXACT 0x20

vmem_t *
vmem_init(vmem_t *vmp, /* optional, may point to pre-allocated vmem */
	  char *name, /* descriptive name */
	  void *base, /* start of initial span */
	  size_t size, /* size of initial span */
	  size_t quantum, /* unit of currency */
	  void *(*afunc)(vmem_t *, size_t, int), /* import alloc function */
	  void (*ffunc)(vmem_t *, void *, size_t), /* import free function */
	  vmem_t *source, /* import source arena */
	  size_t qcache_max, /* maximum size to cache */
	  int vmflag);

/* Destroys arena vmp */
void vmem_destroy(vmem_t *vmp);

/* Allocates size bytes from vmp. Returns the allocated address on success, NULL
 * on failure. vmem_alloc() fails only when VM_NOSLEEP is specified and no
 * resources are currently available. vmflag may specify an allocation policy
 * (VM_BESTFIT or VM_INSTANTFIT). If no policy is defined, VM_INSTANTFIT is used
 * by default. */
void *vmem_alloc(vmem_t *vmp, size_t size, int vmflag);

/* Allocates size bytes at offset phase from an align boundary such that the
 * resulting segment [addr, addr+size) is a subset of [minaddr, maxaddr) that
 * does not straddle a nocross-aligned boundary. vmflag is as above. One
 * performance caveat: if either minaddr or maxaddr is non-NULL, vmem may not be
 * able to satisfy the alloaction in constant time. If allocations within a
 * given [minaddr,maxaddr) range are common it is more efficient to declare that
 * range to be its own arena and use unconstrained allocations on the new arena.
 */
void *vmem_xalloc(vmem_t *vmp, size_t size, size_t align, size_t phase,
		  size_t nocross, void *minaddr, void *maxaddr, int vmflag);

/* Frees size bytes at addr to arena vmp */
void vmem_free(vmem_t *vmp, void *addr, size_t size);

/* Frees size bytes at addr to arena vmp, where addr was a constrained
 * allocation. vmem_xfree() must be used if the original allocation was a
 * vmem_xalloc() because both routines bypass the quantum caches. */
void vmem_xfree(vmem_t *vmp, void *addr, size_t size);

/* Adds the span [addr, addr + size) to arena vmp. Returns addr on success, NULL
 * on failure. vmem_add() will fail only if vmflag is VM_NOSLEEP and no
 * resources are currently available. */
void *vmem_add(vmem_t *vmp, void *addr, size_t size, int vmflag);

void vmem_dump(const vmem_t *vmem);
