#pragma once

#include <yak/vm/pmap.h>

struct vm_map {
	struct pmap pmap;
};

extern struct vm_map kernel_map;
