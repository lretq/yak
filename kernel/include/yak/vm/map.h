#pragma once

#include <yak/vm/pmap.h>
#include <yak/status.h>

struct vm_map {
	struct pmap pmap;
};

struct vm_map *kmap();

status_t vm_map_alloc(struct vm_map *map, size_t length, uintptr_t *out);
