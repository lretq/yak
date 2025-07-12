#pragma once

#include <stdint.h>
#include <stddef.h>

struct cpumask {
	uint64_t bits[1]; // for now 64 cpus
};

extern struct cpumask cpumask_active;

void cpu_init();
void cpu_up(size_t id);

size_t cpus_online();

#define for_each_cpu(index, mask)                                           \
	for (uint64_t tmp_var_ =                                            \
		     __atomic_load_n(&((mask)->bits[0]), __ATOMIC_RELAXED); \
	     tmp_var_ != 0 && ((index) = __builtin_ctz(tmp_var_), 1);       \
	     tmp_var_ &= (tmp_var_ - 1))
